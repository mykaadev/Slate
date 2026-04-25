#include "Modes/Slate/SlateEditorMode.h"

#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        const char* MonthName(int month)
        {
            static constexpr std::array<const char*, 12> Names{{
                "January", "February", "March",     "April",   "May",      "June",
                "July",    "August",   "September", "October", "November", "December",
            }};
            if (month < 1 || month > 12)
            {
                return "Month";
            }
            return Names[static_cast<std::size_t>(month - 1)];
        }

        void CenterCursorForWidthInRegion(float regionStartX, float regionWidth, float itemWidth)
        {
            const float x = regionStartX + std::max(0.0f, (regionWidth - itemWidth) * 0.5f);
            ImGui::SetCursorPosX(x);
        }

        struct EditorColumnLayout
        {
            float pageWidth = 0.0f;
            float pageOffset = 0.0f;
            float outlineWidth = 0.0f;
            float previewWidth = 0.0f;
            float outlineGap = 0.0f;
            float previewGap = 0.0f;
            float gap = 18.0f;
        };

        float ApproachValue(float current, float target, double deltaSeconds, float response = 12.0f)
        {
            const float dt = std::clamp(static_cast<float>(deltaSeconds), 0.0f, 0.12f);
            const float amount = 1.0f - std::exp(-dt * response);
            const float next = current + (target - current) * std::clamp(amount, 0.0f, 1.0f);
            return std::abs(next - target) < 0.0025f ? target : next;
        }

        float SmoothProgress(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        EditorColumnLayout BuildEditorColumnLayout(float availableWidth,
                                                   float configuredWidth,
                                                   float outlineProgress,
                                                   float previewProgress,
                                                   int panelLayout)
        {
            EditorColumnLayout layout;
            const float width = std::max(1.0f, availableWidth);
            const float desiredPageWidth = std::min(width, configuredWidth > 0.0f ? configuredWidth : width);
            const float outlineT = SmoothProgress(outlineProgress);
            const float previewT = SmoothProgress(previewProgress);
            const bool hasOutline = outlineT > 0.001f;
            const bool hasPreview = previewT > 0.001f;
            const float outlineTargetWidth = hasOutline ? std::min(300.0f, std::max(200.0f, width * 0.22f)) : 0.0f;
            const float desiredOutlineWidth = outlineTargetWidth * outlineT;

            if (panelLayout == 1 && (hasOutline || hasPreview))
            {
                layout.outlineWidth = desiredOutlineWidth;
                layout.outlineGap = hasOutline ? layout.gap * outlineT : 0.0f;
                const float maxOutlineWidth = width > 520.0f ? width * 0.34f : width * 0.28f;
                layout.outlineWidth = std::min(layout.outlineWidth, std::max(0.0f, maxOutlineWidth));
                layout.previewGap = hasPreview ? layout.gap * previewT : 0.0f;

                const float laneStart = layout.outlineWidth + layout.outlineGap;
                const float laneWidth = std::max(1.0f, width - laneStart);
                if (hasPreview)
                {
                    const float previewTargetWidth = std::min(420.0f, std::max(240.0f, width * 0.32f));
                    const float previewLimit = std::max(0.0f, laneWidth - layout.previewGap - 1.0f);
                    layout.previewWidth = std::min(previewTargetWidth * previewT, previewLimit * 0.5f);
                }
                layout.pageWidth = std::max(1.0f, laneWidth - layout.previewGap - layout.previewWidth);
                layout.pageOffset = laneStart;
                return layout;
            }

            const float desiredPreviewWidth = hasPreview
                                                  ? std::min(380.0f, std::max(240.0f, width * 0.28f)) * previewT
                                                  : 0.0f;
            layout.outlineGap = hasOutline ? layout.gap * outlineT : 0.0f;
            layout.previewGap = hasPreview ? layout.gap * previewT : 0.0f;
            const float requiredMargin = std::max(hasOutline ? desiredOutlineWidth + layout.outlineGap : 0.0f,
                                                  hasPreview ? desiredPreviewWidth + layout.previewGap : 0.0f);
            const float maxPageWidth = requiredMargin > 0.0f && width > 520.0f
                                           ? std::max(320.0f, width - requiredMargin * 2.0f)
                                           : width;

            layout.pageWidth = std::clamp(std::min(desiredPageWidth, maxPageWidth), 1.0f, width);
            layout.pageOffset = std::max(0.0f, (width - layout.pageWidth) * 0.5f);
            layout.outlineWidth = hasOutline
                                      ? std::min(desiredOutlineWidth, std::max(0.0f, layout.pageOffset - layout.outlineGap))
                                      : 0.0f;
            layout.previewWidth = hasPreview
                                      ? std::min(desiredPreviewWidth,
                                                 std::max(0.0f, width - layout.pageOffset - layout.pageWidth - layout.previewGap))
                                      : 0.0f;
            return layout;
        }

        float MotionResponse(int motion)
        {
            switch (motion)
            {
            case 0:
                return 0.0f;
            case 1:
                return 20.0f;
            case 3:
                return 7.5f;
            default:
                return 12.0f;
            }
        }

        float ScrollResponse(int motion)
        {
            switch (motion)
            {
            case 0:
                return 0.0f;
            case 2:
                return 7.5f;
            default:
                return 15.0f;
            }
        }

        float EditorFontScale(const Software::Slate::EditorSettings& settings)
        {
            const float baseFontSize = ImGui::GetFontSize() > 1.0f ? ImGui::GetFontSize() : 18.0f;
            return std::clamp(static_cast<float>(settings.fontSize) / std::max(1.0f, baseFontSize), 0.65f, 1.50f);
        }

        float EditorLineSpacing(const Software::Slate::EditorSettings& settings)
        {
            return static_cast<float>(std::max(0, settings.lineSpacing));
        }

        int CurrentCaretLine(Software::Slate::SlateEditorContext& editor)
        {
            if (editor.NativeEditorAvailable())
            {
                const auto scrollState = editor.NativeEditorScrollState();
                if (scrollState.totalLineCount > 0)
                {
                    return std::clamp(scrollState.caretLine + 1, 1, std::max(1, scrollState.totalLineCount));
                }
            }

            return static_cast<int>(editor.Editor().ActiveLine()) + 1;
        }

        int PreviewFollowLine(Software::Slate::SlateEditorContext& editor, int followMode)
        {
            if (followMode == 0)
            {
                return CurrentCaretLine(editor);
            }

            if (editor.NativeEditorAvailable())
            {
                const auto scrollState = editor.NativeEditorScrollState();
                if (scrollState.totalLineCount > 0)
                {
                    const int totalLineCount = std::max(1, scrollState.totalLineCount);
                    const int firstVisible = std::clamp(scrollState.firstVisibleLine, 0, totalLineCount - 1);
                    const int visibleCount = std::max(1, scrollState.visibleLineCount);
                    const int scrollLine = std::clamp(firstVisible + visibleCount / 2, 0, totalLineCount - 1);
                    const int caretLine = std::clamp(scrollState.caretLine, 0, totalLineCount - 1);
                    if (followMode == 1)
                    {
                        return caretLine + 1;
                    }
                    if (followMode == 2)
                    {
                        return scrollLine + 1;
                    }

                    const bool caretVisible = caretLine >= firstVisible && caretLine <= firstVisible + visibleCount;
                    const int targetLine = caretVisible
                                               ? static_cast<int>(std::lround(caretLine * 0.72f + scrollLine * 0.28f))
                                               : scrollLine;
                    return std::clamp(targetLine + 1, 1, totalLineCount);
                }
            }

            return CurrentCaretLine(editor);
        }

        std::size_t CurrentHeadingIndex(const std::vector<Software::Slate::Heading>& headings, int currentLine)
        {
            std::size_t current = headings.size();
            for (std::size_t i = 0; i < headings.size(); ++i)
            {
                if (headings[i].line > static_cast<std::size_t>(std::max(1, currentLine)))
                {
                    break;
                }
                current = i;
            }
            return current;
        }

        std::vector<std::string> SplitTableRow(std::string_view line)
        {
            std::string text = Software::Slate::PathUtils::Trim(line);
            if (!text.empty() && text.front() == '|')
            {
                text.erase(text.begin());
            }
            if (!text.empty() && text.back() == '|')
            {
                text.pop_back();
            }

            std::vector<std::string> cells;
            std::size_t start = 0;
            while (start <= text.size())
            {
                const std::size_t end = text.find('|', start);
                const std::string_view cell = end == std::string::npos
                                                  ? std::string_view(text).substr(start)
                                                  : std::string_view(text).substr(start, end - start);
                cells.push_back(Software::Slate::PathUtils::Trim(cell));
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return cells;
        }

        bool IsTableSeparator(std::string_view line)
        {
            const auto cells = SplitTableRow(line);
            if (cells.empty())
            {
                return false;
            }

            bool sawRule = false;
            for (std::string cell : cells)
            {
                cell = Software::Slate::PathUtils::Trim(cell);
                if (!cell.empty() && cell.front() == ':')
                {
                    cell.erase(cell.begin());
                }
                if (!cell.empty() && cell.back() == ':')
                {
                    cell.pop_back();
                }
                cell = Software::Slate::PathUtils::Trim(cell);
                if (cell.size() < 3)
                {
                    return false;
                }
                for (const char ch : cell)
                {
                    if (ch != '-')
                    {
                        return false;
                    }
                }
                sawRule = true;
            }
            return sawRule;
        }

        bool IsTableRow(std::string_view line)
        {
            return line.find('|') != std::string_view::npos && SplitTableRow(line).size() >= 2;
        }

        bool ParseMarkdownImage(std::string_view line, std::string* alt, std::string* target)
        {
            const std::size_t open = line.find("![");
            if (open == std::string_view::npos)
            {
                return false;
            }

            const std::size_t closeLabel = line.find(']', open + 2);
            if (closeLabel == std::string_view::npos || closeLabel + 1 >= line.size() || line[closeLabel + 1] != '(')
            {
                return false;
            }

            const std::size_t closeTarget = line.find(')', closeLabel + 2);
            if (closeTarget == std::string_view::npos)
            {
                return false;
            }

            if (alt)
            {
                *alt = std::string(line.substr(open + 2, closeLabel - open - 2));
            }
            if (target)
            {
                std::string raw = Software::Slate::PathUtils::Trim(line.substr(closeLabel + 2,
                                                                               closeTarget - closeLabel - 2));
                if (!raw.empty() && raw.front() == '<')
                {
                    const std::size_t close = raw.find('>');
                    raw = close == std::string::npos ? raw.substr(1) : raw.substr(1, close - 1);
                }
                else
                {
                    const auto space = std::find_if(raw.begin(), raw.end(), [](unsigned char ch) {
                        return std::isspace(ch) != 0;
                    });
                    raw.erase(space, raw.end());
                }
                *target = raw;
            }
            return true;
        }

        bool IsWebUrl(std::string_view target)
        {
            return target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0;
        }

        Software::Slate::fs::path ResolveMarkdownImageTarget(const Software::Slate::WorkspaceService& workspace,
                                                             const Software::Slate::fs::path& documentRelativePath,
                                                             std::string_view target)
        {
            std::string text = Software::Slate::PathUtils::Trim(target);
            if (text.empty() || IsWebUrl(text))
            {
                return {};
            }

            std::replace(text.begin(), text.end(), '\\', '/');
            if (!text.empty() && text.front() == '/')
            {
                return (workspace.Root() / text.substr(1)).lexically_normal();
            }

            Software::Slate::fs::path path(text);
            if (path.is_absolute())
            {
                return path.lexically_normal();
            }

            return (workspace.Root() / documentRelativePath.parent_path() / path).lexically_normal();
        }

        std::size_t DrawMarkdownTableBlock(const std::vector<std::string>& lines, std::size_t start, int lineSpacing)
        {
            const auto headers = SplitTableRow(lines[start]);
            const int columnCount = static_cast<int>(headers.size());
            ImGui::PushID(static_cast<int>(start));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                                ImVec2(10.0f, 4.0f + static_cast<float>(lineSpacing) * 0.5f));
            if (ImGui::BeginTable("MarkdownPreviewTable", columnCount,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
            {
                for (int column = 0; column < columnCount; ++column)
                {
                    ImGui::TableSetupColumn(nullptr);
                }

                ImGui::TableNextRow();
                for (int column = 0; column < columnCount; ++column)
                {
                    ImGui::TableSetColumnIndex(column);
                    DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(headers[static_cast<std::size_t>(column)]),
                                    MarkdownTable);
                }

                std::size_t cursor = start + 2;
                while (cursor < lines.size() && IsTableRow(lines[cursor]) && !Software::Slate::PathUtils::Trim(lines[cursor]).empty())
                {
                    const auto cells = SplitTableRow(lines[cursor]);
                    ImGui::TableNextRow();
                    for (int column = 0; column < columnCount; ++column)
                    {
                        ImGui::TableSetColumnIndex(column);
                        if (static_cast<std::size_t>(column) < cells.size())
                        {
                            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(cells[static_cast<std::size_t>(column)]));
                        }
                    }
                    ++cursor;
                }

                ImGui::EndTable();
                ImGui::PopStyleVar();
                ImGui::PopID();
                return cursor;
            }

            ImGui::PopStyleVar();
            ImGui::PopID();
            return start + 1;
        }

        std::string JournalDateLabel(const Software::Slate::JournalService& journal,
                                     const Software::Slate::fs::path& relativePath)
        {
            int year = 0;
            int month = 0;
            int day = 0;
            if (!journal.ParseJournalDate(relativePath, &year, &month, &day))
            {
                return {};
            }

            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%d %s %d", day, MonthName(month), year);
            return buffer;
        }
    }

    const char* SlateEditorMode::ModeName() const
    {
        return Software::Slate::ModeIds::Editor;
    }

    bool SlateEditorMode::WantsNativeEditorVisible(const Software::Core::Runtime::AppContext& context) const
    {
        auto& mutableContext = const_cast<Software::Core::Runtime::AppContext&>(context);
        const auto& ui = UiState(mutableContext);
        const auto& workspace = WorkspaceContext(mutableContext);
        return EditorContext(mutableContext).NativeEditorAvailable() &&
               workspace.HasWorkspaceLoaded() &&
               workspace.Documents().HasOpenDocument() &&
               ui.editorView == Software::Slate::SlateEditorView::Document &&
               !HelpOpen() &&
               !SearchOpen() &&
               !PromptOpen() &&
               !ConfirmOpen();
    }

    void SlateEditorMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        auto& shortcuts = workspace.Shortcuts();
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);
        const auto& editorSettings = workspace.CurrentEditorSettings();
        editor.SetNativeEditorSettings(editorSettings);
        if (!workspace.HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto* document = workspace.Documents().Active();
        if (!document)
        {
            ShowHome(context);
            return;
        }

        std::string dropError;
        const int inserted = editor.ProcessDroppedFiles(context.droppedFiles,
                                                        workspace.Documents(),
                                                        workspace.Assets(),
                                                        context.frame.elapsedSeconds,
                                                        &dropError);
        if (inserted > 0)
        {
            SetStatus("inserted " + std::to_string(inserted) + " image link(s)");
        }
        else if (!dropError.empty())
        {
            SetError(dropError);
        }

        const auto pasteClipboardImage = [&]() {
            Software::Slate::fs::path assetRelative;
            std::string error;
            if (workspace.Assets().SaveClipboardImageAsset(document->relativePath, &assetRelative, &error))
            {
                editor.InsertTextAtCursor(workspace.Documents(),
                                          Software::Slate::AssetService::MarkdownImageLink(document->relativePath,
                                                                                           assetRelative) +
                                              document->lineEnding,
                                          context.frame.elapsedSeconds);
                SetStatus("pasted image");
                return true;
            }

            SetError(error);
            return false;
        };

        const auto toggleOutlinePanel = [&]() {
            ui.editorView = Software::Slate::SlateEditorView::Document;
            ui.outlinePanelOpen = !ui.outlinePanelOpen;
            ui.navigation.Reset();
        };

        const auto togglePreviewPanel = [&]() {
            editor.CommitToActiveDocument(workspace.Documents(), context.frame.elapsedSeconds);
            ui.editorView = Software::Slate::SlateEditorView::Document;
            ui.previewPanelOpen = !ui.previewPanelOpen;
        };

        if (handleInput)
        {
            if (ui.editorView == Software::Slate::SlateEditorView::Outline)
            {
                ui.visibleHeadings = editor.ParseHeadings(workspace.Documents());
                ui.navigation.SetCount(ui.visibleHeadings.size());
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateDown, true))
                {
                    ui.navigation.MoveNext();
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateUp, true))
                {
                    ui.navigation.MovePrevious();
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::Accept))
                {
                    if (ui.navigation.HasSelection() && !ui.visibleHeadings.empty())
                    {
                        editor.JumpToLine(ui.visibleHeadings[ui.navigation.Selected()].line);
                    }
                    ui.editorView = Software::Slate::SlateEditorView::Document;
                }
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorOutline) || shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
                {
                    ui.editorView = Software::Slate::SlateEditorView::Document;
                }
            }
            else if (ui.editorView == Software::Slate::SlateEditorView::Preview)
            {
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorPreview) || shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
                {
                    ui.editorView = Software::Slate::SlateEditorView::Document;
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorOutline))
                {
                    ui.navigation.Reset();
                    ui.editorView = Software::Slate::SlateEditorView::Outline;
                }
            }
            else
            {
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::Save))
                {
                    SaveActiveDocument(context);
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::Find) &&
                    workspace.Documents().HasOpenDocument())
                {
                    BeginSearchOverlay(true, context, SearchOverlayScope::Document);
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::PasteClipboardImage) &&
                    workspace.Documents().HasOpenDocument())
                {
                    pasteClipboardImage();
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::Escape))
                {
                    editor.ReleaseNativeEditorFocus();
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::Outline))
                {
                    toggleOutlinePanel();
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::Preview))
                {
                    togglePreviewPanel();
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::Todo))
                {
                    BeginTodoCreate(context, true);
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::NavigateBack))
                {
                    NavigateDocumentHistory(context, -1);
                }
                if (editor.ConsumeNativeCommand(Software::Slate::NativeEditorCommand::NavigateForward))
                {
                    NavigateDocumentHistory(context, 1);
                }

                if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorFind) && workspace.Documents().HasOpenDocument())
                {
                    BeginSearchOverlay(true, context, SearchOverlayScope::Document);
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorOutline))
                {
                    toggleOutlinePanel();
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorPreview))
                {
                    togglePreviewPanel();
                }
                else if (editorSettings.pasteClipboardImages &&
                         IsCtrlPressed(ImGuiKey_V) && workspace.Assets().HasClipboardImage() && workspace.Documents().HasOpenDocument())
                {
                    pasteClipboardImage();
                }
                else if (editor.IsTextFocused())
                {
                    if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
                    {
                        editor.SetTextFocused(false);
                    }
                }
                else
                {
                    if (shortcuts.Pressed(Software::Slate::ShortcutAction::BrowserFilter))
                    {
                        BeginSearchOverlay(true, context);
                    }
                    else if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
                    {
                        ShowHome(context);
                        return;
                    }
                }
            }
        }

        if (ui.editorView == Software::Slate::SlateEditorView::Outline)
        {
            DrawOutline(context);
        }
        else if (ui.editorView == Software::Slate::SlateEditorView::Preview)
        {
            DrawMarkdownPreview(context, *document);
        }
        else
        {
            DrawEditor(context, *document);
        }
    }

    std::string SlateEditorMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        auto& mutableContext = const_cast<Software::Core::Runtime::AppContext&>(context);
        const auto& ui = UiState(mutableContext);
        const auto& shortcuts = WorkspaceContext(mutableContext).Shortcuts();
        const std::string move = "(" + shortcuts.Label(Software::Slate::ShortcutAction::NavigateUp) + "/" +
                                 shortcuts.Label(Software::Slate::ShortcutAction::NavigateDown) + ") move";
        if (ui.editorView == Software::Slate::SlateEditorView::Outline)
        {
            return move + "   " + shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "jump") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "editor");
        }
        if (ui.editorView == Software::Slate::SlateEditorView::Preview)
        {
            return shortcuts.Helper(Software::Slate::ShortcutAction::EditorPreview, "edit") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::EditorOutline, "outline") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "editor");
        }
        return shortcuts.Helper(Software::Slate::ShortcutAction::EditorPreview, "preview") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::EditorOutline, "outline") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::EditorFind, "find") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeTodos, "todos") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::HomeSettings, "config") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::EditorSave, "save") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
    }

    void SlateEditorMode::DrawEditor(Software::Core::Runtime::AppContext& context,
                                     Software::Slate::DocumentService::Document& document)
    {
        auto& editor = EditorContext(context);
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        const auto& editorSettings = workspace.CurrentEditorSettings();
        const float panelResponse = MotionResponse(editorSettings.panelMotion);
        if (panelResponse <= 0.0f)
        {
            ui.outlinePanelProgress = ui.outlinePanelOpen ? 1.0f : 0.0f;
            ui.previewPanelProgress = ui.previewPanelOpen ? 1.0f : 0.0f;
        }
        else
        {
            ui.outlinePanelProgress = ApproachValue(ui.outlinePanelProgress,
                                                    ui.outlinePanelOpen ? 1.0f : 0.0f,
                                                    context.frame.deltaSeconds,
                                                    panelResponse);
            ui.previewPanelProgress = ApproachValue(ui.previewPanelProgress,
                                                    ui.previewPanelOpen ? 1.0f : 0.0f,
                                                    context.frame.deltaSeconds,
                                                    panelResponse);
        }

        ImGui::TextColored(Muted, "%s", document.relativePath.generic_string().c_str());
        ImGui::SameLine();
        ImGui::TextColored(document.conflict ? Red : (document.dirty ? Amber : Green), "%s",
                           document.conflict ? "external change" : (document.dirty ? "dirty" : "saved"));

        const float contentStartX = ImGui::GetCursorPosX();
        const ImVec2 initialAvail = ImGui::GetContentRegionAvail();
        const float initialConfiguredWidth = editorSettings.pageWidth > 0
                                                 ? static_cast<float>(editorSettings.pageWidth)
                                                 : initialAvail.x;
        const EditorColumnLayout initialLayout = BuildEditorColumnLayout(initialAvail.x,
                                                                         initialConfiguredWidth,
                                                                         ui.outlinePanelProgress,
                                                                         ui.previewPanelProgress,
                                                                         editorSettings.panelLayout);
        const float writingTextInset = 16.0f;

        if (editor.Journal().IsJournalPath(document.relativePath))
        {
            ImGui::Dummy(ImVec2(1.0f, 10.0f));
            ImGui::SetCursorPosX(contentStartX);
            DrawJournalCompanion(context,
                                 document,
                                 initialAvail.x,
                                 initialLayout.pageOffset + writingTextInset,
                                 std::max(1.0f, initialLayout.pageWidth - writingTextInset));
            ImGui::Dummy(ImVec2(1.0f, 12.0f));
            ImGui::SetCursorPosX(contentStartX);
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float configuredWidth = editorSettings.pageWidth > 0
                                          ? static_cast<float>(editorSettings.pageWidth)
                                          : avail.x;
        const EditorColumnLayout layout = BuildEditorColumnLayout(avail.x,
                                                                  configuredWidth,
                                                                  ui.outlinePanelProgress,
                                                                  ui.previewPanelProgress,
                                                                  editorSettings.panelLayout);
        const float pageHeight = std::max(1.0f, avail.y - 52.0f);
        const float rowStartY = ImGui::GetCursorPosY();
        const float pageX = contentStartX + layout.pageOffset;
        const int previewFollowLine = PreviewFollowLine(editor, editorSettings.previewFollowMode);

        if (layout.outlineWidth > 1.0f)
        {
            ImGui::SetCursorPos(ImVec2(pageX - layout.outlineGap - layout.outlineWidth, rowStartY));
            ImGui::BeginChild("OutlineSidePanel", ImVec2(layout.outlineWidth, pageHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);
            DrawOutlinePanel(context);
            ImGui::EndChild();
        }

        if (layout.previewWidth > 1.0f)
        {
            ImGui::SetCursorPos(ImVec2(pageX + layout.pageWidth + layout.previewGap, rowStartY));
            ImGui::BeginChild("PreviewSidePanel", ImVec2(layout.previewWidth, pageHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawMarkdownPreviewContent(context, document, layout.previewWidth);
            const float maxScrollY = ImGui::GetScrollMaxY();
            if (maxScrollY <= 1.0f)
            {
                ui.previewScrollY = 0.0f;
                ui.previewScrollTargetY = 0.0f;
                ui.previewLastFollowLine = -1;
            }
            else
            {
                const float currentScrollY = ImGui::GetScrollY();
                const bool previewHovered = ImGui::IsWindowHovered();
                const float wheel = ImGui::GetIO().MouseWheel;
                if (previewHovered && wheel != 0.0f)
                {
                    const float wheelStep = std::max(42.0f, ImGui::GetTextLineHeightWithSpacing() * 5.0f);
                    ui.previewScrollTargetY = std::clamp(ui.previewScrollTargetY - wheel * wheelStep, 0.0f, maxScrollY);
                    ui.previewManualScrollUntil = context.frame.elapsedSeconds + 120.0;
                    ui.previewLastFollowLine = previewFollowLine;
                }
                else if (previewHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                         std::abs(currentScrollY - ui.previewScrollY) > 2.0f)
                {
                    ui.previewScrollTargetY = std::clamp(currentScrollY, 0.0f, maxScrollY);
                    ui.previewManualScrollUntil = context.frame.elapsedSeconds + 120.0;
                    ui.previewLastFollowLine = previewFollowLine;
                }

                const bool manualScrollHold = context.frame.elapsedSeconds < ui.previewManualScrollUntil &&
                                              previewFollowLine == ui.previewLastFollowLine;
                if (editorSettings.previewFollowMode != 0 &&
                    !manualScrollHold)
                {
                    const int lineCount = static_cast<int>(
                        std::max<std::size_t>(1, Software::Slate::MarkdownService::SplitLines(document.text).size()));
                    const float normalizedLine =
                        static_cast<float>(std::clamp(previewFollowLine - 1, 0, std::max(1, lineCount - 1))) /
                        static_cast<float>(std::max(1, lineCount - 1));
                    ui.previewScrollTargetY = std::clamp(maxScrollY * normalizedLine, 0.0f, maxScrollY);
                    ui.previewLastFollowLine = previewFollowLine;
                }

                const float scrollResponse = ScrollResponse(editorSettings.scrollMotion);
                const float targetScrollY = std::clamp(ui.previewScrollTargetY, 0.0f, maxScrollY);
                ui.previewScrollY = scrollResponse <= 0.0f
                                        ? targetScrollY
                                        : ApproachValue(currentScrollY,
                                                        targetScrollY,
                                                        context.frame.deltaSeconds,
                                                        scrollResponse);
                ui.previewScrollY = std::clamp(ui.previewScrollY, 0.0f, maxScrollY);
                ImGui::SetScrollY(ui.previewScrollY);
            }
            ImGui::EndChild();
        }

        ImGui::SetCursorPos(ImVec2(pageX, rowStartY));
        if (editor.NativeEditorAvailable())
        {
            const float nativeScrollbarWidth = 10.0f;
            const float nativeScrollbarGap = 6.0f;
            const float nativeEditorWidth = std::max(1.0f, layout.pageWidth - nativeScrollbarWidth - nativeScrollbarGap);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Background);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::BeginChild("NativeMarkdownEditorPage", ImVec2(nativeEditorWidth, pageHeight), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const ImVec2 editorPos = ImGui::GetWindowPos();
            const ImVec2 editorSize = ImGui::GetWindowSize();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            if (editor.NativeEditorVisible())
            {
                editor.RenderNativeEditor(document,
                                          workspace.Documents(),
                                          context.frame.elapsedSeconds,
                                          editorPos.x,
                                          editorPos.y,
                                          editorSize.x,
                                          editorSize.y);
            }

            ImGui::SetCursorPos(ImVec2(pageX + nativeEditorWidth + nativeScrollbarGap, rowStartY));
            int targetFirstLine = 0;
            const auto scrollState = editor.NativeEditorScrollState();
            if (DrawVerticalDocumentScrollbar("NativeEditorScrollbar",
                                              ImVec2(nativeScrollbarWidth, pageHeight),
                                              scrollState.firstVisibleLine,
                                              scrollState.visibleLineCount,
                                              scrollState.totalLineCount,
                                              &targetFirstLine,
                                              editorSettings.scrollbarStyle))
            {
                const float scrollResponse = ScrollResponse(editorSettings.scrollMotion);
                if (scrollResponse <= 0.0f)
                {
                    ui.nativeEditorScrollTargetActive = false;
                    editor.SetNativeEditorFirstVisibleLine(targetFirstLine);
                }
                else
                {
                    ui.nativeEditorScrollTargetLine = static_cast<float>(targetFirstLine);
                    ui.nativeEditorScrollLine = static_cast<float>(scrollState.firstVisibleLine);
                    ui.nativeEditorScrollTargetActive = true;
                }
            }
            if (ui.nativeEditorScrollTargetActive)
            {
                const float scrollResponse = ScrollResponse(editorSettings.scrollMotion);
                const float nextLine = scrollResponse <= 0.0f
                                           ? ui.nativeEditorScrollTargetLine
                                           : ApproachValue(static_cast<float>(scrollState.firstVisibleLine),
                                                           ui.nativeEditorScrollTargetLine,
                                                           context.frame.deltaSeconds,
                                                           scrollResponse);
                ui.nativeEditorScrollLine = nextLine;
                editor.SetNativeEditorFirstVisibleLine(static_cast<int>(std::lround(nextLine)));
                if (std::abs(nextLine - ui.nativeEditorScrollTargetLine) < 0.5f)
                {
                    ui.nativeEditorScrollTargetActive = false;
                }
            }

            ImGui::SetCursorPos(ImVec2(contentStartX, rowStartY + pageHeight));
            return;
        }

        ImGui::BeginChild("LiveMarkdownEditorPage", ImVec2(layout.pageWidth, pageHeight), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        DrawLiveMarkdownEditor(context, document);
        ImGui::EndChild();
        ImGui::SetCursorPos(ImVec2(contentStartX, rowStartY + pageHeight));
    }

    void SlateEditorMode::DrawOutline(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        auto& editor = EditorContext(context);
        ui.visibleHeadings = editor.ParseHeadings(WorkspaceContext(context).Documents());
        ui.navigation.SetCount(ui.visibleHeadings.size());
        const std::size_t currentHeading = CurrentHeadingIndex(ui.visibleHeadings, CurrentCaretLine(editor));

        ImGui::TextColored(Cyan, "outline");
        ImGui::Separator();

        if (ui.visibleHeadings.empty())
        {
            ImGui::TextColored(Muted, "no headings");
            return;
        }

        for (std::size_t i = 0; i < ui.visibleHeadings.size(); ++i)
        {
            const auto& heading = ui.visibleHeadings[i];
            const bool selected = i == ui.navigation.Selected();
            const bool current = i == currentHeading;
            std::string indent(static_cast<std::size_t>(std::max(0, heading.level - 1)) * 2, ' ');
            ImGui::TextColored(selected ? Green : (current ? Cyan : Primary),
                               "%s %s%s  line %zu%s",
                               selected ? ">" : (current ? "*" : " "),
                               indent.c_str(),
                               heading.title.c_str(),
                               heading.line,
                               current ? "  current" : "");
        }
    }

    void SlateEditorMode::DrawOutlinePanel(Software::Core::Runtime::AppContext& context)
    {
        auto& editor = EditorContext(context);
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        ui.visibleHeadings = editor.ParseHeadings(workspace.Documents());
        const std::size_t currentHeading = CurrentHeadingIndex(ui.visibleHeadings, CurrentCaretLine(editor));

        ImGui::TextColored(Cyan, "outline");
        ImGui::Separator();

        if (ui.visibleHeadings.empty())
        {
            ImGui::TextColored(Muted, "no headings");
            return;
        }

        for (std::size_t i = 0; i < ui.visibleHeadings.size(); ++i)
        {
            const auto& heading = ui.visibleHeadings[i];
            const bool current = i == currentHeading;
            const std::string indent(static_cast<std::size_t>(std::max(0, heading.level - 1)) * 1, ' ');
            ImGui::TextColored(current ? Cyan : Primary,
                               "%s%s%s",
                               current ? "* " : "  ",
                               indent.c_str(),
                               heading.title.c_str());
            if (ImGui::IsItemClicked())
            {
                editor.JumpToLine(heading.line);
                ui.editorView = Software::Slate::SlateEditorView::Document;
            }
            ImGui::SameLine();
            ImGui::TextColored(current ? Green : Muted, "%zu%s", heading.line, current ? " *" : "");
        }
    }

    void SlateEditorMode::DrawJournalCompanion(Software::Core::Runtime::AppContext& context,
                                               const Software::Slate::DocumentService::Document& document,
                                               float layoutWidth,
                                               float textInset,
                                               float requestedTextWidth)
    {
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        const auto* summary = editor.CurrentMonthSummary(workspace.Workspace(), &document, context.frame.elapsedSeconds);
        if (!summary)
        {
            return;
        }

        const std::string dateLabel = JournalDateLabel(editor.Journal(), document.relativePath);
        const float regionStartX = ImGui::GetCursorPosX();
        const float availableWidth = layoutWidth > 1.0f
                                         ? layoutWidth
                                         : ImGui::GetContentRegionAvail().x;
        const float calendarWidth = 176.0f;
        const float calendarHeight = 148.0f;
        const float panelGap = 18.0f;
        const float minGraphWidth = 210.0f;
        const bool stackPanels = availableWidth < calendarWidth + panelGap + minGraphWidth;
        const float graphWidth = stackPanels
                                     ? std::min(availableWidth, 360.0f)
                                     : std::min(420.0f, std::max(minGraphWidth, availableWidth - calendarWidth - panelGap));
        const float graphHeight = 116.0f;

        if (stackPanels)
        {
            CenterCursorForWidthInRegion(regionStartX, availableWidth, calendarWidth);
            ImGui::BeginChild("JournalCalendarPanel", ImVec2(calendarWidth, calendarHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalCalendar(*summary);
            ImGui::EndChild();

            ImGui::Dummy(ImVec2(1.0f, 8.0f));
            CenterCursorForWidthInRegion(regionStartX, availableWidth, graphWidth);
            ImGui::BeginChild("JournalGraphPanel", ImVec2(graphWidth, graphHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalActivityGraph(*summary);
            ImGui::EndChild();
        }
        else
        {
            const float rowWidth = calendarWidth + panelGap + graphWidth;
            CenterCursorForWidthInRegion(regionStartX, availableWidth, rowWidth);
            const float rowStartY = ImGui::GetCursorPosY();
            ImGui::BeginGroup();
            ImGui::BeginChild("JournalCalendarPanel", ImVec2(calendarWidth, calendarHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalCalendar(*summary);
            ImGui::EndChild();
            ImGui::SameLine(0.0f, panelGap);
            ImGui::SetCursorPosY(rowStartY + std::max(0.0f, (calendarHeight - graphHeight) * 0.5f));
            ImGui::BeginChild("JournalGraphPanel", ImVec2(graphWidth, graphHeight), false,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawJournalActivityGraph(*summary);
            ImGui::EndChild();
            ImGui::EndGroup();
            ImGui::SetCursorPosY(rowStartY + calendarHeight);
        }

        ImGui::Dummy(ImVec2(1.0f, 10.0f));
        const float textStartX = regionStartX + std::max(0.0f, textInset);
        const float textWidth = requestedTextWidth > 1.0f
                                    ? requestedTextWidth
                                    : std::max(1.0f, availableWidth - std::max(0.0f, textInset));
        ImGui::SetCursorPosX(textStartX);
        ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + textWidth);
        if (!dateLabel.empty())
        {
            ImGui::TextColored(Cyan, "%s", dateLabel.c_str());
        }
        else
        {
            char title[64];
            std::snprintf(title, sizeof(title), "%s %d", MonthName(summary->month), summary->year);
            ImGui::TextColored(Cyan, "%s", title);
        }
        ImGui::SetCursorPosX(textStartX);
        ImGui::TextColored(Primary, "%s", summary->prompt.c_str());
        ImGui::PopTextWrapPos();
    }

    void SlateEditorMode::DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary)
    {
        static constexpr std::array<const char*, 7> Weekdays{{"M", "T", "W", "T", "F", "S", "S"}};
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, 1.0f));
        if (!ImGui::BeginTable("JournalMonthCalendar", 7,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
                                   ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX))
        {
            ImGui::PopStyleVar();
            return;
        }

        for (int column = 0; column < 7; ++column)
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 24.0f);
        }

        for (const char* weekday : Weekdays)
        {
            ImGui::TableNextColumn();
            ImGui::TextColored(Muted, "%s", weekday);
        }

        int cell = 0;
        int day = 1;
        while (day <= summary.daysInMonth)
        {
            ImGui::TableNextRow();
            for (int column = 0; column < 7; ++column, ++cell)
            {
                ImGui::TableSetColumnIndex(column);
                if (cell < summary.firstWeekdayMondayBased || day > summary.daysInMonth)
                {
                    ImGui::TextUnformatted(" ");
                    continue;
                }

                const auto& daySummary = summary.days[static_cast<std::size_t>(day - 1)];
                ImVec4 dayColor = daySummary.hasContent ? Green : Muted;
                if (day == summary.currentDay)
                {
                    dayColor = Cyan;
                }
                if (day == summary.activeDay)
                {
                    dayColor = Amber;
                }

                char dayBuffer[8];
                std::snprintf(dayBuffer, sizeof(dayBuffer), "%2d", day);
                ImGui::TextColored(dayColor, "%s", dayBuffer);
                ++day;
            }
        }

        ImGui::EndTable();
        ImGui::PopStyleVar();
    }

    void SlateEditorMode::DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary)
    {
        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float labelHeight = 18.0f;
        const float graphHeight = std::max(52.0f, ImGui::GetContentRegionAvail().y - labelHeight);
        const float paddingX = 8.0f;
        const float paddingY = 5.0f;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, graphHeight + labelHeight);
        ImGui::InvisibleButton("JournalActivityGraph", size);

        auto* drawList = ImGui::GetWindowDrawList();
        const float baselineY = start.y + graphHeight;
        drawList->AddLine(ImVec2(start.x + paddingX, baselineY), ImVec2(start.x + width - paddingX, baselineY),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(Muted.x, Muted.y, Muted.z, 0.34f)), 1.0f);

        int maxWords = 0;
        for (const auto& daySummary : summary.days)
        {
            maxWords = std::max(maxWords, daySummary.wordCount);
        }

        std::vector<ImVec2> points;
        points.reserve(summary.days.size());
        const float plotWidth = std::max(1.0f, width - paddingX * 2.0f);
        const float plotHeight = std::max(1.0f, graphHeight - paddingY * 2.0f);
        for (std::size_t i = 0; i < summary.days.size(); ++i)
        {
            const float t = summary.days.size() > 1 ? static_cast<float>(i) / static_cast<float>(summary.days.size() - 1) : 0.0f;
            const float x = start.x + paddingX + plotWidth * t;
            const float normalized = maxWords > 0
                                         ? static_cast<float>(summary.days[i].wordCount) / static_cast<float>(maxWords)
                                         : 0.0f;
            const float y = start.y + paddingY + plotHeight - normalized * plotHeight;
            points.emplace_back(x, y);
        }

        const ImVec4 graphColor = maxWords > 0 ? Green : Muted;
        const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(graphColor);
        if (points.size() > 1)
        {
            const auto drawCurve = [&](ImU32 color, float thickness) {
                for (std::size_t i = 0; i + 1 < points.size(); ++i)
                {
                    const ImVec2 p0 = i == 0 ? points[i] : points[i - 1];
                    const ImVec2 p1 = points[i];
                    const ImVec2 p2 = points[i + 1];
                    const ImVec2 p3 = i + 2 < points.size() ? points[i + 2] : points[i + 1];
                    const ImVec2 c1(p1.x + (p2.x - p0.x) / 6.0f, p1.y + (p2.y - p0.y) / 6.0f);
                    const ImVec2 c2(p2.x - (p3.x - p1.x) / 6.0f, p2.y - (p3.y - p1.y) / 6.0f);
                    drawList->AddBezierCubic(p1, c1, c2, p2, color, thickness, 18);
                }
            };
            drawCurve(lineColor, 2.0f);
        }
        else if (!points.empty())
        {
            drawList->AddCircleFilled(points.front(), 3.0f, lineColor, 16);
        }

        const auto drawMarker = [&](int day, const ImVec4& color, float radius) {
            if (day <= 0 || day > static_cast<int>(points.size()))
            {
                return;
            }
            const ImVec2 point = points[static_cast<std::size_t>(day - 1)];
            drawList->AddCircleFilled(point, radius, ImGui::ColorConvertFloat4ToU32(color), 18);
        };

        drawMarker(summary.currentDay, Cyan, 3.0f);
        drawMarker(summary.activeDay, Amber, 3.5f);

        char label[8];
        std::snprintf(label, sizeof(label), "%d", 1);
        drawList->AddText(ImVec2(start.x + paddingX, baselineY + 2.0f), ImGui::ColorConvertFloat4ToU32(Muted), label);
        std::snprintf(label, sizeof(label), "%d", summary.daysInMonth);
        const ImVec2 endSize = ImGui::CalcTextSize(label);
        drawList->AddText(ImVec2(start.x + width - paddingX - endSize.x, baselineY + 2.0f),
                          ImGui::ColorConvertFloat4ToU32(Muted), label);
    }

    void SlateEditorMode::DrawLiveMarkdownEditor(Software::Core::Runtime::AppContext& context,
                                                 Software::Slate::DocumentService::Document& document)
    {
        auto& editorContext = EditorContext(context);
        auto& editor = editorContext.Editor();
        auto& workspace = WorkspaceContext(context);
        const auto& shortcuts = workspace.Shortcuts();
        const auto& editorSettings = workspace.CurrentEditorSettings();
        editor.EnsureLoaded(document.text, document.lineEnding);
        const auto& lines = editor.Lines();

        bool inCodeFence = false;
        bool inFrontmatter = !lines.empty() && Software::Slate::PathUtils::Trim(lines.front()) == "---";
        bool checkingFrontmatter = inFrontmatter;

        const float configuredWidth = editorSettings.pageWidth > 0
                                          ? static_cast<float>(editorSettings.pageWidth)
                                          : ImGui::GetContentRegionAvail().x;
        const float pageWidth = std::min(ImGui::GetContentRegionAvail().x, configuredWidth);
        const float pageStartX = std::max(0.0f, (ImGui::GetContentRegionAvail().x - pageWidth) * 0.5f);
        const ImGuiStyle& style = ImGui::GetStyle();
        const float fontScale = EditorFontScale(editorSettings);
        ImGui::SetWindowFontScale(fontScale);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(style.ItemSpacing.x, EditorLineSpacing(editorSettings)));
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (pageWidth > 1.0f)
            {
                ImGui::SetCursorPosX(pageStartX);
            }

            const std::string trimmed = Software::Slate::PathUtils::Trim(lines[i]);
            const bool lineIsFrontmatter = checkingFrontmatter;

            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            if (i == editor.ActiveLine())
            {
                if (editor.FocusRequested())
                {
                    ImGui::SetKeyboardFocusHere();
                    editor.ClearFocusRequest();
                }

                ImGui::SetNextItemWidth(pageWidth);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.095f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      MarkdownLineBaseColor(editor.ActiveLineText(), inCodeFence, lineIsFrontmatter));
                int cursor = editor.CaretColumn();
                const int cursorBeforeInput = cursor;
                const int lineSizeBeforeInput = static_cast<int>(editor.ActiveLineText().size());
                const TextInputResult result = InputTextString("##LiveMarkdownLine", editor.ActiveLineText(),
                                                               ImGuiInputTextFlags_EnterReturnsTrue,
                                                               &cursor,
                                                               editor.RequestedCursorColumn());
                editor.SetCaretColumn(cursor);
                editor.ClearRequestedCursorColumn();
                editorContext.SetTextFocused(ImGui::IsItemActive());
                ImGui::PopStyleColor(2);

                if (result.edited)
                {
                    editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                }

                if (result.submitted)
                {
                    if (IsTodoSlashCommand(editor.ActiveLineText()))
                    {
                        BeginTodoCreate(context, true);
                    }
                    else
                    {
                        editor.SplitActiveLine();
                        editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(),
                                                             context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && ImGui::IsKeyPressed(ImGuiKey_Backspace, true) &&
                    cursorBeforeInput <= 0 && editor.ActiveLine() > 0)
                {
                    if (editor.MergeActiveLineWithPrevious())
                    {
                        editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, true) &&
                    cursorBeforeInput >= lineSizeBeforeInput)
                {
                    if (editor.MergeActiveLineWithNext())
                    {
                        editorContext.CommitToActiveDocument(WorkspaceContext(context).Documents(), context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateUp, true) && editor.ActiveLine() > 0)
                {
                    editor.MoveActiveLine(-1);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (editorContext.IsTextFocused() && shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateDown, true) &&
                    editor.ActiveLine() + 1 < lines.size())
                {
                    editor.MoveActiveLine(1);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }
            }
            else
            {
                DrawMarkdownLine(lines[i], inCodeFence, lineIsFrontmatter, editorSettings.showWhitespace, fontScale);
            }

            ImGui::EndGroup();
            if (ImGui::IsItemClicked())
            {
                editor.SetActiveLine(i, static_cast<int>(lines[i].size()));
                editorContext.SetTextFocused(true);
            }
            ImGui::PopID();

            if (checkingFrontmatter && i > 0 && trimmed == "---")
            {
                checkingFrontmatter = false;
            }
            else if (!checkingFrontmatter && (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0))
            {
                inCodeFence = !inCodeFence;
            }
        }
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1.0f);
    }

    void SlateEditorMode::DrawMarkdownPreview(Software::Core::Runtime::AppContext& context,
                                              const Software::Slate::DocumentService::Document& document)
    {
        const auto& workspace = WorkspaceContext(context);
        const auto& editorSettings = workspace.CurrentEditorSettings();

        ImGui::TextColored(Muted, "%s", document.relativePath.generic_string().c_str());
        ImGui::SameLine();
        ImGui::TextColored(document.conflict ? Red : (document.dirty ? Amber : Green), "%s",
                           document.conflict ? "external change" : (document.dirty ? "dirty" : "saved"));

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float configuredWidth = editorSettings.pageWidth > 0
                                          ? static_cast<float>(editorSettings.pageWidth)
                                          : avail.x;
        const float pageWidth = std::min(avail.x, configuredWidth);
        const float pageOffset = std::max(0.0f, (avail.x - pageWidth) * 0.5f);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pageOffset);
        ImGui::BeginChild("MarkdownPreviewPage", ImVec2(pageWidth, std::max(1.0f, avail.y - 52.0f)), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        DrawMarkdownPreviewContent(context, document, pageWidth);
        ImGui::EndChild();
    }

    void SlateEditorMode::DrawMarkdownPreviewContent(Software::Core::Runtime::AppContext& context,
                                                     const Software::Slate::DocumentService::Document& document,
                                                     float contentWidth)
    {
        const auto lines = Software::Slate::MarkdownService::SplitLines(document.text);
        bool inCodeFence = false;
        bool inFrontmatter = !lines.empty() && Software::Slate::PathUtils::Trim(lines.front()) == "---";
        bool checkingFrontmatter = inFrontmatter;
        const auto& workspace = WorkspaceContext(context);
        const auto& editorSettings = workspace.CurrentEditorSettings();
        const float fontScale = EditorFontScale(editorSettings);

        ImGui::SetWindowFontScale(fontScale);
        ImGui::TextColored(Cyan, "preview");
        ImGui::Separator();

        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(style.ItemSpacing.x, EditorLineSpacing(editorSettings)));

        for (std::size_t i = 0; i < lines.size();)
        {
            const std::string trimmed = Software::Slate::PathUtils::Trim(lines[i]);
            const bool lineIsFrontmatter = checkingFrontmatter;

            ImGui::PushID(static_cast<int>(i));
            std::string imageAlt;
            std::string imageTarget;
            const bool startsTable = !inCodeFence && !lineIsFrontmatter && i + 1 < lines.size() &&
                                     IsTableRow(lines[i]) && IsTableSeparator(lines[i + 1]);

            if (startsTable)
            {
                ImGui::PopID();
                i = DrawMarkdownTableBlock(lines, i, editorSettings.lineSpacing);
                continue;
            }
            if (!inCodeFence && !lineIsFrontmatter && ParseMarkdownImage(trimmed, &imageAlt, &imageTarget))
            {
                const auto imagePath = ResolveMarkdownImageTarget(workspace.Workspace(), document.relativePath, imageTarget);
                DrawMarkdownImage(imagePath, imageAlt, imageTarget, contentWidth);
            }
            else
            {
                DrawMarkdownLine(lines[i], inCodeFence, lineIsFrontmatter, editorSettings.showWhitespace, fontScale);
            }
            ImGui::PopID();

            if (checkingFrontmatter && i > 0 && trimmed == "---")
            {
                checkingFrontmatter = false;
            }
            else if (!checkingFrontmatter && (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0))
            {
                inCodeFence = !inCodeFence;
            }

            ++i;
        }

        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1.0f);
    }
}
