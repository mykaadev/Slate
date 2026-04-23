#pragma once

#include "App/Slate/WorkspaceTree.h"

#include "imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate::UI
{
    inline ImVec4 Background{0.055f, 0.055f, 0.052f, 1.0f};
    inline ImVec4 Primary{0.86f, 0.84f, 0.77f, 1.0f};
    inline ImVec4 Muted{0.48f, 0.48f, 0.43f, 1.0f};
    inline ImVec4 Cyan{0.50f, 0.78f, 0.86f, 1.0f};
    inline ImVec4 Amber{0.93f, 0.70f, 0.35f, 1.0f};
    inline ImVec4 Green{0.58f, 0.82f, 0.56f, 1.0f};
    inline ImVec4 Red{0.92f, 0.38f, 0.36f, 1.0f};
    inline ImVec4 Panel{0.085f, 0.085f, 0.08f, 1.0f};
    inline ImVec4 Code{0.70f, 0.75f, 0.78f, 1.0f};

    inline ImVec4 MarkdownHeading1{0.56f, 0.84f, 0.90f, 1.0f};
    inline ImVec4 MarkdownHeading2{0.70f, 0.88f, 0.68f, 1.0f};
    inline ImVec4 MarkdownHeading3{0.94f, 0.73f, 0.42f, 1.0f};
    inline ImVec4 MarkdownBullet{0.84f, 0.69f, 0.42f, 1.0f};
    inline ImVec4 MarkdownCheckbox{0.88f, 0.74f, 0.45f, 1.0f};
    inline ImVec4 MarkdownCheckboxDone{0.57f, 0.86f, 0.60f, 1.0f};
    inline ImVec4 MarkdownQuote{0.63f, 0.72f, 0.78f, 1.0f};
    inline ImVec4 MarkdownTable{0.78f, 0.74f, 0.56f, 1.0f};
    inline ImVec4 MarkdownLink{0.50f, 0.80f, 0.96f, 1.0f};
    inline ImVec4 MarkdownImage{0.92f, 0.62f, 0.51f, 1.0f};
    inline ImVec4 MarkdownBold{0.96f, 0.92f, 0.84f, 1.0f};
    inline ImVec4 MarkdownItalic{0.76f, 0.80f, 0.67f, 1.0f};
    inline ImVec4 MarkdownCode{0.78f, 0.83f, 0.86f, 1.0f};

    struct TextInputResult
    {
        bool edited = false;
        bool submitted = false;
    };

    TextInputResult InputTextString(const char* label, std::string& text, ImGuiInputTextFlags flags,
                                    int* cursorPos = nullptr, int requestedCursorPos = -1);
    TextInputResult InputTextMultilineString(const char* label, std::string& text, const ImVec2& size,
                                             ImGuiInputTextFlags flags, int* cursorPos = nullptr);
    float CenteredColumnWidth(float maxWidth = 760.0f, float minMargin = 0.0f);
    void SetCursorCenteredForWidth(float width);
    void PushSlateScrollbarStyle(int style = 1);
    void PopSlateScrollbarStyle();
    bool DrawVerticalDocumentScrollbar(const char* id, const ImVec2& size, int firstVisibleLine,
                                       int visibleLineCount, int totalLineCount, int* targetFirstVisibleLine,
                                       int style = 1);
    void TextCentered(const char* text, const ImVec4& color = Primary);
    void TextLine(const char* key, const char* label);
    float ShortcutTextWidth(std::string_view text);
    void DrawShortcutText(std::string_view text, const ImVec4& keyColor = Amber, const ImVec4& labelColor = Primary);
    void DrawShortcutTextCentered(std::string_view text, const ImVec4& keyColor = Amber,
                                  const ImVec4& labelColor = Primary);
    bool ContainsFilter(const std::string& value, const char* filter);
    bool IsShiftQuestion();
    std::string DisplayNameForTreeRow(const TreeViewRow& row);
    void DrawInlineSpans(const std::vector<MarkdownInlineSpan>& spans, const ImVec4& baseColor = Primary,
                         bool showWhitespace = false);
    void DrawMarkdownLine(const std::string& line, bool inCodeFence, bool inFrontmatter = false,
                          bool showWhitespace = false, float fontScale = 1.0f);
    void DrawMarkdownImage(const fs::path& absolutePath, std::string_view alt, std::string_view target,
                           float maxWidth);
}
