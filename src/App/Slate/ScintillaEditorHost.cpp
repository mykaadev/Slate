#include "App/Slate/ScintillaEditorHost.h"

#include "App/Slate/UI/SlateUi.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>

#include <SciLexer.h>
#include <Scintilla.h>

#include <ILexer.h>
#include <LexerModule.h>

extern const Lexilla::LexerModule lmMarkdown;
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        bool IsHorizontalWhitespace(char ch)
        {
            return ch == ' ' || ch == '\t';
        }

        std::string_view TrimWhitespace(std::string_view text)
        {
            std::size_t start = 0;
            while (start < text.size() && IsHorizontalWhitespace(text[start]))
            {
                ++start;
            }

            std::size_t end = text.size();
            while (end > start && IsHorizontalWhitespace(text[end - 1]))
            {
                --end;
            }

            return text.substr(start, end - start);
        }

        std::string StripLineEnding(std::string text)
        {
            while (!text.empty() && (text.back() == '\0' || text.back() == '\n' || text.back() == '\r'))
            {
                text.pop_back();
            }
            return text;
        }

        struct SmartEnterAction
        {
            bool replaceCurrentLine = false;
            std::string text;
        };

        bool BuildSmartEnterAction(std::string_view line, const std::string& lineEnding, SmartEnterAction* action)
        {
            if (!action)
            {
                return false;
            }

            std::size_t cursor = 0;
            while (cursor < line.size() && IsHorizontalWhitespace(line[cursor]))
            {
                ++cursor;
            }

            const std::string indent(line.substr(0, cursor));
            std::string quoteMarkers;

            while (cursor < line.size() && line[cursor] == '>')
            {
                quoteMarkers += '>';
                ++cursor;

                while (cursor < line.size() && IsHorizontalWhitespace(line[cursor]))
                {
                    ++cursor;
                }
            }

            std::string blockPrefix = indent;
            if (!quoteMarkers.empty())
            {
                blockPrefix += quoteMarkers + " ";
            }

            const std::size_t contentStart = cursor;
            const auto consumeWhitespace = [&line](std::size_t* index) {
                while (*index < line.size() && IsHorizontalWhitespace(line[*index]))
                {
                    ++(*index);
                }
            };

            if (cursor < line.size() && (line[cursor] == '-' || line[cursor] == '*' || line[cursor] == '+'))
            {
                const char bullet = line[cursor];
                ++cursor;
                if (cursor >= line.size() || !IsHorizontalWhitespace(line[cursor]))
                {
                    return false;
                }
                consumeWhitespace(&cursor);

                std::string continuation = blockPrefix + bullet + " ";
                if (cursor + 2 < line.size() &&
                    line[cursor] == '[' &&
                    (line[cursor + 1] == ' ' || line[cursor + 1] == 'x' || line[cursor + 1] == 'X') &&
                    line[cursor + 2] == ']')
                {
                    cursor += 3;
                    if (cursor < line.size() && IsHorizontalWhitespace(line[cursor]))
                    {
                        consumeWhitespace(&cursor);
                    }
                    continuation += "[ ] ";
                }

                if (TrimWhitespace(line.substr(cursor)).empty())
                {
                    action->replaceCurrentLine = true;
                    action->text = blockPrefix;
                }
                else
                {
                    action->replaceCurrentLine = false;
                    action->text = lineEnding + continuation;
                }
                return true;
            }

            if (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor])) != 0)
            {
                const std::size_t numberStart = cursor;
                while (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor])) != 0)
                {
                    ++cursor;
                }

                if (cursor < line.size() && (line[cursor] == '.' || line[cursor] == ')'))
                {
                    const std::size_t separatorIndex = cursor;
                    const char separator = line[cursor];
                    ++cursor;
                    if (cursor >= line.size() || !IsHorizontalWhitespace(line[cursor]))
                    {
                        return false;
                    }
                    consumeWhitespace(&cursor);

                    long long number = 1;
                    try
                    {
                        number = std::stoll(std::string(line.substr(numberStart, separatorIndex - numberStart)));
                    }
                    catch (...)
                    {
                        number = 1;
                    }

                    if (TrimWhitespace(line.substr(cursor)).empty())
                    {
                        action->replaceCurrentLine = true;
                        action->text = blockPrefix;
                    }
                    else
                    {
                        action->replaceCurrentLine = false;
                        action->text = lineEnding + blockPrefix + std::to_string(number + 1) + separator + " ";
                    }
                    return true;
                }
            }

            if (blockPrefix.size() > indent.size())
            {
                if (TrimWhitespace(line.substr(contentStart)).empty())
                {
                    action->replaceCurrentLine = true;
                    action->text = indent;
                }
                else
                {
                    action->replaceCurrentLine = false;
                    action->text = lineEnding + blockPrefix;
                }
                return true;
            }

            return false;
        }

        std::uint32_t BlendByte(float value)
        {
            return static_cast<std::uint32_t>(std::clamp(std::lround(value * 255.0f), 0l, 255l));
        }

#if defined(_WIN32)
        COLORREF ToColorRef(const ImVec4& color)
        {
            return RGB(BlendByte(color.x), BlendByte(color.y), BlendByte(color.z));
        }

        ImVec4 MixColor(const ImVec4& base, const ImVec4& accent, float amount)
        {
            const float t = std::clamp(amount, 0.0f, 1.0f);
            return {
                base.x + (accent.x - base.x) * t,
                base.y + (accent.y - base.y) * t,
                base.z + (accent.z - base.z) * t,
                1.0f,
            };
        }

        int ToEolMode(std::string_view lineEnding)
        {
            if (lineEnding == "\r\n")
            {
                return SC_EOL_CRLF;
            }
            if (lineEnding == "\r")
            {
                return SC_EOL_CR;
            }
            return SC_EOL_LF;
        }

        constexpr wchar_t HostPropertyName[] = L"SlateScintillaHost";
#endif
    }

    ScintillaEditorHost::ScintillaEditorHost() = default;

    ScintillaEditorHost::~ScintillaEditorHost()
    {
        DestroyWindowResources();
    }

    void ScintillaEditorHost::AttachToParentWindow(void* nativeHandle)
    {
#if defined(_WIN32)
        if (m_parentWindow == nativeHandle)
        {
            return;
        }

        DestroyWindowResources();
        m_parentWindow = nativeHandle;
#else
        (void)nativeHandle;
#endif
    }

    bool ScintillaEditorHost::Available() const
    {
#if defined(_WIN32)
        return m_parentWindow != nullptr;
#else
        return false;
#endif
    }

    bool ScintillaEditorHost::Visible() const
    {
        return m_visible;
    }

    void ScintillaEditorHost::SetVisible(bool visible)
    {
        m_visible = visible;
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return;
        }

        ::ShowWindow(static_cast<HWND>(m_editorWindow), visible ? SW_SHOW : SW_HIDE);
#endif
    }

    void ScintillaEditorHost::Clear()
    {
        m_loadedPath.clear();
        m_lastSyncedText.clear();
        m_lineEnding = "\n";

#if defined(_WIN32)
        if (!EnsureWindow())
        {
            return;
        }

        SetText("");
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_EMPTYUNDOBUFFER, 0, 0);
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_SETSAVEPOINT, 0, 0);
#endif
    }

    void ScintillaEditorHost::LoadDocument(const DocumentService::Document* document, bool forceReload)
    {
#if defined(_WIN32)
        if (!document || !EnsureWindow())
        {
            return;
        }

        const bool pathChanged = m_loadedPath != document->absolutePath;
        const bool cleanEditor = ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_GETMODIFY, 0, 0) == 0;
        const bool cleanTextChanged = cleanEditor && m_lastSyncedText != document->text;
        if (!forceReload && !pathChanged && !cleanTextChanged)
        {
            return;
        }

        m_loadedPath = document->absolutePath;
        m_lastSyncedText = document->text;
        m_lineEnding = document->lineEnding.empty() ? "\n" : document->lineEnding;

        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_SETEOLMODE, ToEolMode(m_lineEnding), 0);
        SetText(document->text);
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_COLOURISE, 0, -1);
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_EMPTYUNDOBUFFER, 0, 0);
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_SETSAVEPOINT, 0, 0);
#else
        (void)document;
        (void)forceReload;
#endif
    }

    void ScintillaEditorHost::Render(const DocumentService::Document& document,
                                     DocumentService& documents,
                                     double elapsedSeconds,
                                     float screenX,
                                     float screenY,
                                     float width,
                                     float height)
    {
#if defined(_WIN32)
        if (!EnsureWindow())
        {
            return;
        }

        LoadDocument(&document, false);
        ApplyThemeIfNeeded();
        UpdateGeometry(screenX, screenY, width, height);
        SetVisible(true);
        SyncDocument(documents, elapsedSeconds);
#else
        (void)document;
        (void)documents;
        (void)elapsedSeconds;
        (void)screenX;
        (void)screenY;
        (void)width;
        (void)height;
#endif
    }

    bool ScintillaEditorHost::SyncDocument(DocumentService& documents, double elapsedSeconds)
    {
#if defined(_WIN32)
        auto* document = documents.Active();
        if (!document || !m_editorWindow || m_loadedPath != document->absolutePath)
        {
            return false;
        }

        const bool clean = ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_GETMODIFY, 0, 0) == 0;
        if (clean && document->text == m_lastSyncedText)
        {
            return false;
        }

        const std::string currentText = GetText();
        if (currentText == document->text)
        {
            m_lastSyncedText = currentText;
            return false;
        }

        document->text = currentText;
        document->lineEnding = m_lineEnding;
        documents.MarkEdited(elapsedSeconds);
        m_lastSyncedText = currentText;
        return true;
#else
        (void)documents;
        (void)elapsedSeconds;
        return false;
#endif
    }

    void ScintillaEditorHost::MarkSaved()
    {
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return;
        }

        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_SETSAVEPOINT, 0, 0);
        m_lastSyncedText = GetText();
#endif
    }

    bool ScintillaEditorHost::IsFocused() const
    {
#if defined(_WIN32)
        return m_editorWindow && ::GetFocus() == static_cast<HWND>(m_editorWindow);
#else
        return false;
#endif
    }

    void ScintillaEditorHost::Focus()
    {
#if defined(_WIN32)
        if (EnsureWindow())
        {
            ::SetFocus(static_cast<HWND>(m_editorWindow));
        }
#endif
    }

    void ScintillaEditorHost::ReleaseFocus()
    {
#if defined(_WIN32)
        if (m_parentWindow)
        {
            ::SetFocus(static_cast<HWND>(m_parentWindow));
        }
#endif
    }

    void ScintillaEditorHost::SetEditorSettings(const EditorSettings& settings)
    {
        m_editorSettings = EditorSettingsService::Sanitize(settings);
    }

    void ScintillaEditorHost::JumpToLine(std::size_t line)
    {
#if defined(_WIN32)
        if (!EnsureWindow())
        {
            return;
        }

        const sptr_t lineIndex = static_cast<sptr_t>(line > 0 ? line - 1 : 0);
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_GOTOLINE, static_cast<WPARAM>(lineIndex), 0);
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_ENSUREVISIBLEENFORCEPOLICY,
                       static_cast<WPARAM>(lineIndex), 0);
        Focus();
#else
        (void)line;
#endif
    }

    bool ScintillaEditorHost::InsertTextAtCursor(const std::string& text)
    {
#if defined(_WIN32)
        if (text.empty() || !EnsureWindow())
        {
            return false;
        }

        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_REPLACESEL, 0,
                       reinterpret_cast<LPARAM>(text.c_str()));
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_COLOURISE, 0, -1);
        Focus();
        return true;
#else
        (void)text;
        return false;
#endif
    }

    bool ScintillaEditorHost::HandleSmartEnter()
    {
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return false;
        }

        const auto call = [this](unsigned int message, uintptr_t wParam = 0, intptr_t lParam = 0) {
            return ::SendMessageW(static_cast<HWND>(m_editorWindow), message, static_cast<WPARAM>(wParam),
                                  static_cast<LPARAM>(lParam));
        };

        const auto currentPosition = static_cast<sptr_t>(call(SCI_GETCURRENTPOS, 0, 0));
        if (currentPosition != static_cast<sptr_t>(call(SCI_GETANCHOR, 0, 0)))
        {
            return false;
        }

        const auto lineIndex = static_cast<sptr_t>(call(SCI_LINEFROMPOSITION,
                                                        static_cast<uintptr_t>(currentPosition), 0));
        const auto lineStart = static_cast<sptr_t>(call(SCI_POSITIONFROMLINE,
                                                        static_cast<uintptr_t>(lineIndex), 0));
        const auto lineEnd = static_cast<sptr_t>(call(SCI_GETLINEENDPOSITION,
                                                      static_cast<uintptr_t>(lineIndex), 0));
        if (currentPosition != lineEnd)
        {
            return false;
        }

        const auto lineLength = static_cast<sptr_t>(call(SCI_LINELENGTH, static_cast<uintptr_t>(lineIndex), 0));
        if (lineLength <= 0)
        {
            return false;
        }

        std::string lineText(static_cast<std::size_t>(lineLength) + 1, '\0');
        call(SCI_GETLINE, static_cast<uintptr_t>(lineIndex), reinterpret_cast<intptr_t>(lineText.data()));
        lineText = StripLineEnding(std::move(lineText));

        SmartEnterAction action;
        if (!BuildSmartEnterAction(lineText, m_lineEnding, &action))
        {
            return false;
        }

        call(SCI_BEGINUNDOACTION, 0, 0);
        if (action.replaceCurrentLine)
        {
            call(SCI_SETTARGETSTART, static_cast<uintptr_t>(lineStart), 0);
            call(SCI_SETTARGETEND, static_cast<uintptr_t>(lineEnd), 0);
            call(SCI_REPLACETARGET, static_cast<uintptr_t>(action.text.size()),
                 reinterpret_cast<intptr_t>(action.text.c_str()));
            const auto newPosition = lineStart + static_cast<sptr_t>(action.text.size());
            call(SCI_GOTOPOS, static_cast<uintptr_t>(newPosition), 0);
        }
        else
        {
            call(SCI_REPLACESEL, 0, reinterpret_cast<intptr_t>(action.text.c_str()));
        }
        call(SCI_SCROLLCARET, 0, 0);
        call(SCI_ENDUNDOACTION, 0, 0);
        return true;
#else
        return false;
#endif
    }

    bool ScintillaEditorHost::ConsumeCommand(NativeEditorCommand command)
    {
        const std::uint32_t mask = CommandMask(command);
        const bool hasCommand = (m_pendingCommands & mask) != 0;
        m_pendingCommands &= ~mask;
        return hasCommand;
    }

    bool ScintillaEditorHost::EnsureWindow()
    {
#if defined(_WIN32)
        if (m_editorWindow)
        {
            return true;
        }

        if (!m_parentWindow)
        {
            return false;
        }

        static bool scintillaRegistered = false;
        if (!scintillaRegistered)
        {
            scintillaRegistered = Scintilla_RegisterClasses(::GetModuleHandleW(nullptr)) != 0;
        }
        if (!scintillaRegistered)
        {
            return false;
        }

        HWND window = ::CreateWindowExW(0, L"Scintilla", L"",
                                        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS,
                                        0, 0, 1, 1,
                                        static_cast<HWND>(m_parentWindow),
                                        nullptr,
                                        ::GetModuleHandleW(nullptr),
                                        nullptr);
        if (!window)
        {
            return false;
        }

        m_editorWindow = window;
        ::SetPropW(window, HostPropertyName, this);
        m_editorProc = reinterpret_cast<void*>(::SetWindowLongPtrW(
            window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ScintillaEditorHost::EditorWindowProc)));

        ConfigureEditor();
        SetVisible(m_visible);
        return true;
#else
        return false;
#endif
    }

    void ScintillaEditorHost::DestroyWindowResources()
    {
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return;
        }

        HWND window = static_cast<HWND>(m_editorWindow);
        if (m_editorProc)
        {
            ::SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_editorProc));
        }
        ::RemovePropW(window, HostPropertyName);
        ::DestroyWindow(window);

        m_editorWindow = nullptr;
        m_editorProc = nullptr;
#endif
    }

    void ScintillaEditorHost::ConfigureEditor()
    {
#if defined(_WIN32)
        const auto call = [this](unsigned int message, uintptr_t wParam = 0, intptr_t lParam = 0) {
            return ::SendMessageW(static_cast<HWND>(m_editorWindow), message, static_cast<WPARAM>(wParam),
                                  static_cast<LPARAM>(lParam));
        };

        call(SCI_SETCODEPAGE, SC_CP_UTF8, 0);
        call(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE, 0);
        call(SCI_SETFONTQUALITY, SC_EFF_QUALITY_LCD_OPTIMIZED, 0);
        call(SCI_SETMULTIPLESELECTION, 1, 0);
        call(SCI_SETADDITIONALSELECTIONTYPING, 1, 0);
        call(SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH, 0);
        call(SCI_SETCARETWIDTH, 2, 0);
        call(SCI_SETMARGINWIDTHN, 0, 0);
        call(SCI_SETMARGINWIDTHN, 1, 0);
        call(SCI_SETMARGINWIDTHN, 2, 0);
        call(SCI_SETMARGINWIDTHN, 3, 0);
        call(SCI_SETMARGINLEFT, 0, 16);
        call(SCI_SETMARGINRIGHT, 0, 16);
        call(SCI_SETLAYOUTCACHE, SC_CACHE_PAGE, 0);
        call(SCI_SETEXTRAASCENT, 3, 0);
        call(SCI_SETEXTRADESCENT, 3, 0);
        call(SCI_SETUSETABS, 0, 0);
        call(SCI_SETTABWIDTH, 4, 0);
        call(SCI_SETINDENT, 4, 0);
        call(SCI_SETTABINDENTS, 1, 0);
        call(SCI_SETBACKSPACEUNINDENTS, 1, 0);
        call(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("fold"), reinterpret_cast<LPARAM>("0"));
        call(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("lexer.markdown.header.eolfill"),
             reinterpret_cast<LPARAM>("1"));
        call(SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(lmMarkdown.Create()));

#endif
    }

    void ScintillaEditorHost::ApplyThemeIfNeeded()
    {
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return;
        }

        const COLORREF background = ToColorRef(UI::Background);
        const COLORREF primary = ToColorRef(UI::Primary);
        const COLORREF muted = ToColorRef(UI::Muted);
        const COLORREF cyan = ToColorRef(UI::Cyan);
        const COLORREF panel = ToColorRef(UI::Panel);
        const COLORREF selection = ToColorRef(MixColor(UI::Background, UI::Cyan, 0.20f));
        const COLORREF caretLine = ToColorRef(MixColor(UI::Background, UI::Panel, 0.72f));

        const auto call = [this](unsigned int message, uintptr_t wParam = 0, intptr_t lParam = 0) {
            return ::SendMessageW(static_cast<HWND>(m_editorWindow), message, static_cast<WPARAM>(wParam),
                                  static_cast<LPARAM>(lParam));
        };

        const int fontSize = m_editorSettings.fontSize;
        const int lineSpacing = m_editorSettings.lineSpacing;
        const int tabWidth = m_editorSettings.tabWidth;

        call(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>("Cascadia Mono"));
        call(SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
        call(SCI_STYLESETFORE, STYLE_DEFAULT, primary);
        call(SCI_STYLESETBACK, STYLE_DEFAULT, background);
        call(SCI_STYLECLEARALL, 0, 0);

        call(SCI_SETCARETFORE, primary, 0);
        call(SCI_SETCARETLINEBACK, caretLine, 0);
        call(SCI_SETCARETLINEVISIBLE, m_editorSettings.highlightCurrentLine ? 1 : 0, 0);
        call(SCI_SETSELBACK, 1, selection);
        call(SCI_SETSELFORE, 0, 0);
        call(SCI_SETWHITESPACEFORE, 1, muted);
        call(SCI_SETWHITESPACEBACK, 1, background);
        call(SCI_SETWRAPMODE, m_editorSettings.wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
        call(SCI_SETHSCROLLBAR, m_editorSettings.wordWrap ? 0 : 1, 0);
        call(SCI_SETEXTRAASCENT, lineSpacing, 0);
        call(SCI_SETEXTRADESCENT, lineSpacing, 0);
        call(SCI_SETUSETABS, m_editorSettings.indentWithTabs ? 1 : 0, 0);
        call(SCI_SETTABWIDTH, tabWidth, 0);
        call(SCI_SETINDENT, tabWidth, 0);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_DEFAULT, primary);
        call(SCI_STYLESETBACK, SCE_MARKDOWN_DEFAULT, background);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_LINE_BEGIN, muted);
        call(SCI_STYLESETFORE, SCE_MARKDOWN_PRECHAR, muted);
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HRULE, muted);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_STRONG1, ToColorRef(UI::MarkdownBold));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_STRONG2, ToColorRef(UI::MarkdownBold));
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG1, 1);
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG2, 1);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_EM1, ToColorRef(UI::MarkdownItalic));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_EM2, ToColorRef(UI::MarkdownItalic));
        call(SCI_STYLESETITALIC, SCE_MARKDOWN_EM1, 1);
        call(SCI_STYLESETITALIC, SCE_MARKDOWN_EM2, 1);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER1, ToColorRef(UI::MarkdownHeading1));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER2, ToColorRef(UI::MarkdownHeading2));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER3, ToColorRef(UI::MarkdownHeading3));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER4, cyan);
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER5, ToColorRef(UI::MarkdownTable));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER6, muted);
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER1, 1);
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER2, 1);
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER3, 1);
        call(SCI_STYLESETSIZE, SCE_MARKDOWN_HEADER1, fontSize + 6);
        call(SCI_STYLESETSIZE, SCE_MARKDOWN_HEADER2, fontSize + 3);
        call(SCI_STYLESETSIZE, SCE_MARKDOWN_HEADER3, fontSize + 1);
        call(SCI_STYLESETSIZE, SCE_MARKDOWN_HEADER4, fontSize);
        call(SCI_STYLESETSIZE, SCE_MARKDOWN_HEADER5, fontSize);
        call(SCI_STYLESETSIZE, SCE_MARKDOWN_HEADER6, fontSize);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_ULIST_ITEM, ToColorRef(UI::MarkdownBullet));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_OLIST_ITEM, ToColorRef(UI::MarkdownBullet));
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_ULIST_ITEM, 1);
        call(SCI_STYLESETBOLD, SCE_MARKDOWN_OLIST_ITEM, 1);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_BLOCKQUOTE, ToColorRef(UI::MarkdownQuote));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_LINK, ToColorRef(UI::MarkdownLink));
        call(SCI_STYLESETUNDERLINE, SCE_MARKDOWN_LINK, 1);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_CODE, ToColorRef(UI::MarkdownCode));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_CODE2, ToColorRef(UI::MarkdownCode));
        call(SCI_STYLESETFORE, SCE_MARKDOWN_CODEBK, ToColorRef(UI::MarkdownCode));
        call(SCI_STYLESETBACK, SCE_MARKDOWN_CODE, panel);
        call(SCI_STYLESETBACK, SCE_MARKDOWN_CODE2, panel);
        call(SCI_STYLESETBACK, SCE_MARKDOWN_CODEBK, panel);
        call(SCI_STYLESETEOLFILLED, SCE_MARKDOWN_CODEBK, 1);

        call(SCI_STYLESETFORE, SCE_MARKDOWN_STRIKEOUT, muted);
        call(SCI_STYLESETFORE, SCE_MARKDOWN_HRULE, muted);

        call(SCI_COLOURISE, 0, -1);
#endif
    }

    void ScintillaEditorHost::UpdateGeometry(float screenX, float screenY, float width, float height)
    {
#if defined(_WIN32)
        if (!m_editorWindow || !m_parentWindow)
        {
            return;
        }

        const int targetWidth = std::max(1, static_cast<int>(std::lround(width)));
        const int targetHeight = std::max(1, static_cast<int>(std::lround(height)));
        const POINT topLeft{
            static_cast<LONG>(std::lround(screenX)),
            static_cast<LONG>(std::lround(screenY)),
        };

        if (topLeft.x == m_lastX && topLeft.y == m_lastY &&
            targetWidth == m_lastWidth && targetHeight == m_lastHeight)
        {
            return;
        }

        m_lastX = topLeft.x;
        m_lastY = topLeft.y;
        m_lastWidth = targetWidth;
        m_lastHeight = targetHeight;

        ::SetWindowPos(static_cast<HWND>(m_editorWindow), HWND_TOP,
                       m_lastX, m_lastY, m_lastWidth, m_lastHeight,
                       SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
        (void)screenX;
        (void)screenY;
        (void)width;
        (void)height;
#endif
    }

    std::string ScintillaEditorHost::GetText() const
    {
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return {};
        }

        const auto length = static_cast<std::size_t>(
            ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_GETLENGTH, 0, 0));
        std::string text(length + 1, '\0');
        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_GETTEXT,
                       static_cast<WPARAM>(length + 1),
                       reinterpret_cast<LPARAM>(text.data()));
        text.resize(length);
        return text;
#else
        return {};
#endif
    }

    void ScintillaEditorHost::SetText(const std::string& text)
    {
#if defined(_WIN32)
        if (!m_editorWindow)
        {
            return;
        }

        ::SendMessageW(static_cast<HWND>(m_editorWindow), SCI_SETTEXT, 0,
                       reinterpret_cast<LPARAM>(text.c_str()));
#else
        (void)text;
#endif
    }

    std::uint32_t ScintillaEditorHost::CommandMask(NativeEditorCommand command)
    {
        return static_cast<std::uint32_t>(command);
    }

#if defined(_WIN32)
    intptr_t __stdcall ScintillaEditorHost::EditorWindowProc(void* window,
                                                             unsigned int message,
                                                             uintptr_t wParam,
                                                             intptr_t lParam)
    {
        auto* host = static_cast<ScintillaEditorHost*>(::GetPropW(static_cast<HWND>(window), HostPropertyName));
        if (!host)
        {
            return ::DefWindowProcW(static_cast<HWND>(window), message,
                                    static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
        }

        return host->HandleWindowMessage(window, message, wParam, lParam);
    }

    intptr_t ScintillaEditorHost::HandleWindowMessage(void* window,
                                                      unsigned int message,
                                                      uintptr_t wParam,
                                                      intptr_t lParam)
    {
        const bool ctrlPressed = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool altPressed = (::GetKeyState(VK_MENU) & 0x8000) != 0;
        if (message == WM_KEYDOWN)
        {
            if (ctrlPressed && (wParam == 'S' || wParam == 's'))
            {
                m_pendingCommands |= CommandMask(NativeEditorCommand::Save);
                return 0;
            }
            if (ctrlPressed && (wParam == 'F' || wParam == 'f'))
            {
                m_pendingCommands |= CommandMask(NativeEditorCommand::Find);
                return 0;
            }
            if (m_editorSettings.pasteClipboardImages &&
                ctrlPressed && (wParam == 'V' || wParam == 'v') && ::IsClipboardFormatAvailable(CF_DIB) != 0)
            {
                m_pendingCommands |= CommandMask(NativeEditorCommand::PasteClipboardImage);
                return 0;
            }
            if (wParam == VK_ESCAPE)
            {
                m_pendingCommands |= CommandMask(NativeEditorCommand::Escape);
                return 0;
            }
            if (m_editorSettings.autoListContinuation &&
                !ctrlPressed && !altPressed && wParam == VK_RETURN && HandleSmartEnter())
            {
                return 0;
            }
        }
        else if (message == WM_CHAR)
        {
            const bool controlCharacter = wParam < 32;
            const bool allowCommonControls = wParam == '\b' || wParam == '\t' || wParam == '\n' || wParam == '\r';
            if ((ctrlPressed && !altPressed && controlCharacter) || wParam == 27 || (controlCharacter && !allowCommonControls))
            {
                return 0;
            }
        }

        return ::CallWindowProcW(reinterpret_cast<WNDPROC>(m_editorProc),
                                 static_cast<HWND>(window),
                                 message,
                                 static_cast<WPARAM>(wParam),
                                 static_cast<LPARAM>(lParam));
    }
#endif
}
