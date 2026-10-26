#include "Modes/Slate/SlateEditorMode.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

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

        int CurrentCaretLine(Software::Slate::SlateEditorContext& editor)
        {
            std::size_t line = 1;
            editor.CaretPosition(&line, nullptr);
            return static_cast<int>(line);
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

        ImVec4 WithAlpha(ImVec4 color, float alpha)
        {
            color.w = alpha;
            return color;
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
        return workspace.HasWorkspaceLoaded() && workspace.Documents().HasOpenDocument() &&
               ui.editorView == Software::Slate::SlateEditorView::Document;
    }

    std::string SlateEditorMode::StatusBadge(const Software::Core::Runtime::AppContext& context) const
    {
        auto& mutableContext = const_cast<Software::Core::Runtime::AppContext&>(context);
        if (UiState(mutableContext).editorView == Software::Slate::SlateEditorView::Preview)
        {
            return "READ";
        }
        return EditorContext(mutableContext).IsTextFocused() ? "EDIT" : "NOTE";
    }

    void SlateEditorMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        auto& shortcuts = workspace.Shortcuts();
        auto& editor = EditorContext(context);
        auto& ui = UiState(context);
        const auto& editorSettings = workspace.CurrentEditorSettings();
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
        const int inserted = editor.ProcessDroppedFiles(
            context.droppedFiles,
            workspace.Documents(),
            workspace.Assets(),
            static_cast<Software::Slate::ImageStoragePolicy>(editorSettings.imageStoragePolicy),
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
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorOutline) ||
                    shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
                {
                    ui.editorView = Software::Slate::SlateEditorView::Document;
                }
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorPreview))
            {
                // Reading view renders everything and hides the caret
                ui.editorView = ui.editorView == Software::Slate::SlateEditorView::Preview
                                    ? Software::Slate::SlateEditorView::Document
                                    : Software::Slate::SlateEditorView::Preview;
                editor.SetTextFocused(ui.editorView == Software::Slate::SlateEditorView::Document);
                SetStatus(ui.editorView == Software::Slate::SlateEditorView::Preview ? "reading view" : "editing");
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorOutline))
            {
                ui.outlinePanelOpen = !ui.outlinePanelOpen;
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorSidebar))
            {
                ui.sidebarOpen = !ui.sidebarOpen;
            }
            else if (shortcuts.Pressed(Software::Slate::ShortcutAction::EditorFind))
            {
                BeginSearchOverlay(true, context, SearchOverlayScope::Document);
            }
            else if (editorSettings.pasteClipboardImages && editor.IsTextFocused() && IsCtrlPressed(ImGuiKey_V) &&
                     workspace.Assets().HasClipboardImage())
            {
                std::string markdown;
                std::string error;
                if (workspace.Assets().CreateMarkdownImageFromClipboard(
                        document->relativePath,
                        static_cast<Software::Slate::ImageStoragePolicy>(editorSettings.imageStoragePolicy),
                        &markdown,
                        &error))
                {
                    editor.InsertTextAtCursor(workspace.Documents(), markdown + "\n", context.frame.elapsedSeconds);
                    SetStatus("pasted image");
                }
                else
                {
                    SetError(error);
                }
            }
            else if (!editor.IsTextFocused() && ui.editorView == Software::Slate::SlateEditorView::Document)
            {
                if (shortcuts.Pressed(Software::Slate::ShortcutAction::Accept))
                {
                    editor.SetTextFocused(true);
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::BrowserFilter))
                {
                    BeginSearchOverlay(true, context);
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
                {
                    ShowHome(context);
                    return;
                }
            }
            else if (ui.editorView == Software::Slate::SlateEditorView::Preview &&
                     shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
            {
                ui.editorView = Software::Slate::SlateEditorView::Document;
                editor.SetTextFocused(true);
            }
        }

        if (ui.editorView == Software::Slate::SlateEditorView::Outline)
        {
            DrawHeader(context, *document);
            DrawOutline(context);
            return;
        }

        DrawEditor(context, *document, handleInput);

        // Links and tags clicked inside the editor act once the editor has finished its frame
        if (!m_pendingLink.empty())
        {
            const std::string link = std::move(m_pendingLink);
            m_pendingLink.clear();
            OpenLinkTarget(context, link, m_pendingLinkIsWiki);
        }
        if (!m_pendingTag.empty())
        {
            const std::string tag = std::move(m_pendingTag);
            m_pendingTag.clear();
            if (auto* search = SearchOverlay(context))
            {
                search->SetQuery(tag);
            }
            BeginSearchOverlay(false, context);
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
                   shortcuts.Helper(Software::Slate::ShortcutAction::EditorFind, "find") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "edit");
        }
        if (EditorContext(mutableContext).IsTextFocused())
        {
            return shortcuts.Helper(Software::Slate::ShortcutAction::EditorPreview, "read") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::EditorOutline, "outline") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::EditorFind, "find") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::EditorSave, "save") + "   (ctrl+shift+p) commands   (/) blocks   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "done");
        }
        return shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "write") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::EditorPreview, "read") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::EditorOutline, "outline") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::BrowserFilter, "search") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
    }

    void SlateEditorMode::DrawHeader(Software::Core::Runtime::AppContext& context,
                                     const Software::Slate::DocumentService::Document& document)
    {
        auto& ui = UiState(context);
        auto* drawList = ImGui::GetWindowDrawList();

        // Breadcrumb: folders dim, note name bright
        const auto parent = document.relativePath.parent_path();
        bool first = true;
        for (const auto& part : parent)
        {
            if (!first)
            {
                ImGui::SameLine(0.0f, 0.0f);
            }
            ImGui::TextColored(Muted, "%s", part.u8string().c_str());
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(WithAlpha(Muted, 0.5f), " / ");
            first = false;
        }
        if (!first)
        {
            ImGui::SameLine(0.0f, 0.0f);
        }
        ImGui::TextColored(Primary, "%s", document.relativePath.stem().u8string().c_str());

        // State dot: saved, unsaved, or changed on disk
        ImGui::SameLine(0.0f, 14.0f);
        const ImVec4 stateColor = document.conflict ? Red : (document.dirty ? Amber : Green);
        const char* stateLabel = document.conflict ? "changed on disk" : (document.dirty ? "unsaved" : "saved");
        const ImVec2 dot = ImGui::GetCursorScreenPos();
        drawList->AddCircleFilled(ImVec2(dot.x + 4.0f, dot.y + ImGui::GetTextLineHeight() * 0.55f), 3.5f,
                                  ImGui::ColorConvertFloat4ToU32(stateColor), 12);
        ImGui::Dummy(ImVec2(10.0f, 1.0f));
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(WithAlpha(stateColor, 0.8f), "%s", stateLabel);

        // Right side: view mode and panels
        const char* modeLabel = ui.editorView == Software::Slate::SlateEditorView::Preview ? "reading" : "editing";
        const std::string right = std::string(modeLabel) + (ui.outlinePanelOpen ? "  |  outline" : "");
        const float rightWidth = ImGui::CalcTextSize(right.c_str()).x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 20.0f, ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - rightWidth));
        ImGui::TextColored(Muted, "%s", right.c_str());

        const ImVec2 lineStart(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, ImGui::GetCursorScreenPos().y + 4.0f);
        drawList->AddLine(lineStart, ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x, lineStart.y),
                          ImGui::ColorConvertFloat4ToU32(WithAlpha(Muted, 0.18f)), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, 10.0f));
    }

    Software::Slate::fs::path SlateEditorMode::ResolveImage(Software::Core::Runtime::AppContext& context, const std::string& target)
    {
        auto& workspace = WorkspaceContext(context);
        const auto* document = workspace.Documents().Active();
        if (!document || target.empty() || target.rfind("http", 0) == 0 || target.rfind("data:", 0) == 0)
        {
            return {};
        }
        if (m_imagePathsNote != document->relativePath)
        {
            m_imagePaths.clear();
            m_imagePathsNote = document->relativePath;
        }
        if (const auto it = m_imagePaths.find(target); it != m_imagePaths.end())
        {
            return it->second;
        }

        std::string text = target;
        for (std::size_t pos = text.find("%20"); pos != std::string::npos; pos = text.find("%20"))
        {
            text.replace(pos, 3, " ");
        }
        std::replace(text.begin(), text.end(), '\\', '/');
        const auto root = workspace.Workspace().Root();
        Software::Slate::fs::path resolved;
        std::error_code ec;
        const auto relativeToNote = (root / document->relativePath.parent_path() / Software::Slate::fs::u8path(text)).lexically_normal();
        const auto relativeToRoot = (root / Software::Slate::fs::u8path(text.front() == '/' ? text.substr(1) : text)).lexically_normal();
        if (Software::Slate::fs::exists(relativeToNote, ec))
        {
            resolved = relativeToNote;
        }
        else if (Software::Slate::fs::exists(relativeToRoot, ec))
        {
            resolved = relativeToRoot;
        }
        else
        {
            // Obsidian-style embeds name a file anywhere in the vault
            const std::string wanted = Software::Slate::PathUtils::ToLower(Software::Slate::fs::u8path(text).filename().u8string());
            for (const auto& entry : workspace.Workspace().Entries())
            {
                if (Software::Slate::PathUtils::ToLower(entry.relativePath.filename().u8string()) == wanted)
                {
                    resolved = root / entry.relativePath;
                    break;
                }
            }
        }
        m_imagePaths[target] = resolved;
        return resolved;
    }

    void SlateEditorMode::DrawEditor(Software::Core::Runtime::AppContext& context,
                                     Software::Slate::DocumentService::Document& document,
                                     bool handleInput)
    {
        auto& editor = EditorContext(context);
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        auto& shortcuts = workspace.Shortcuts();
        const auto& editorSettings = workspace.CurrentEditorSettings();
        const bool reading = ui.editorView == Software::Slate::SlateEditorView::Preview;

        const float panelResponse = MotionResponse(editorSettings.panelMotion);
        ui.outlinePanelProgress = panelResponse <= 0.0f
                                      ? (ui.outlinePanelOpen ? 1.0f : 0.0f)
                                      : ApproachValue(ui.outlinePanelProgress, ui.outlinePanelOpen ? 1.0f : 0.0f,
                                                      context.frame.deltaSeconds, panelResponse);

        DrawHeader(context, document);

        // Slash palette state comes from the caret line; it owns arrows, tab, enter, and escape while open
        std::vector<Software::Slate::SlashCommandDefinition> slashCommands;
        if (editor.IsTextFocused() && !reading && !editor.HasSelection())
        {
            std::string line;
            std::size_t caretColumn = 0;
            std::string query;
            if (editor.ActiveLineTextAndCaret(&line, &caretColumn) &&
                Software::Slate::SlashCommandService::QueryFromLineAtCaret(line, caretColumn, &query))
            {
                if (query != m_slashCommandQuery)
                {
                    m_slashCommandQuery = query;
                    m_slashCommandIndex = 0;
                }
                if (!(m_slashCommandSuppressed && query == m_slashCommandSuppressedQuery))
                {
                    m_slashCommandSuppressed = false;
                    slashCommands = Software::Slate::SlashCommandService::Filter(query, ExtensionSlashCommands(context));
                }
            }
            else
            {
                m_slashCommandQuery.clear();
                m_slashCommandSuppressed = false;
            }
        }
        const bool paletteOpen = !slashCommands.empty();
        if (paletteOpen)
        {
            m_slashCommandIndex = std::clamp(m_slashCommandIndex, 0, static_cast<int>(slashCommands.size()) - 1);
        }

        if (handleInput && paletteOpen)
        {
            const int count = static_cast<int>(slashCommands.size());
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
            {
                m_slashCommandIndex = (m_slashCommandIndex + 1) % count;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
            {
                m_slashCommandIndex = (m_slashCommandIndex + count - 1) % count;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                m_slashCommandSuppressed = true;
                m_slashCommandSuppressedQuery = m_slashCommandQuery;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Tab, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))
            {
                const auto selected = slashCommands[static_cast<std::size_t>(m_slashCommandIndex)];
                ApplySlashCommand(context, document, selected);
                return;
            }
        }
        else if (handleInput && editor.IsTextFocused() && shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
        {
            editor.ReleaseFocus();
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float footer = 46.0f;
        const float outlineWidth = std::min(280.0f, std::max(200.0f, avail.x * 0.2f)) * SmoothProgress(ui.outlinePanelProgress);
        const float gap = outlineWidth > 1.0f ? 18.0f * SmoothProgress(ui.outlinePanelProgress) : 0.0f;
        const float bodyHeight = std::max(1.0f, avail.y - footer);
        const float bodyY = ImGui::GetCursorPosY();
        const float bodyX = ImGui::GetCursorPosX();

        if (outlineWidth > 1.0f)
        {
            ImGui::SetCursorPos(ImVec2(bodyX, bodyY));
            ImGui::BeginChild("OutlineSidePanel", ImVec2(outlineWidth, bodyHeight), false, ImGuiWindowFlags_NoBackground);
            DrawOutlinePanel(context);
            ImGui::EndChild();
        }

        // Right sidebar hosts panels contributed by plugins (backlinks, calendar, ...)
        std::vector<const Software::Slate::ViewContribution*> sidebarViews;
        if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>())
        {
            for (const auto& view : registry->Views())
            {
                if (view.location == Software::Slate::ViewLocation::Sidebar && ui.closedSidebarViews.count(view.id) == 0)
                {
                    sidebarViews.push_back(&view);
                }
            }
        }
        const bool sidebarWanted = ui.sidebarOpen && !sidebarViews.empty() && avail.x > 900.0f;
        ui.sidebarProgress = panelResponse <= 0.0f ? (sidebarWanted ? 1.0f : 0.0f)
                                                   : ApproachValue(ui.sidebarProgress, sidebarWanted ? 1.0f : 0.0f,
                                                                   context.frame.deltaSeconds, panelResponse);
        const float sidebarWidth = std::min(320.0f, std::max(240.0f, avail.x * 0.22f)) * SmoothProgress(ui.sidebarProgress);
        const float sidebarGap = sidebarWidth > 1.0f ? 18.0f * SmoothProgress(ui.sidebarProgress) : 0.0f;
        if (sidebarWidth > 1.0f && !sidebarViews.empty())
        {
            ImGui::SetCursorPos(ImVec2(bodyX + avail.x - sidebarWidth, bodyY));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, WithAlpha(Panel, 0.55f));
            ImGui::BeginChild("NoteSidebar", ImVec2(sidebarWidth, bodyHeight), ImGuiChildFlags_AlwaysUseWindowPadding);
            for (const auto* view : sidebarViews)
            {
                ImGui::PushID(view->id.c_str());
                ImGui::TextColored(Muted, "%s", view->title.c_str());
                ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::CalcTextSize("x").x - 2.0f);
                const ImVec2 closePos = ImGui::GetCursorScreenPos();
                const ImVec2 closeSize = ImGui::CalcTextSize("x");
                if (ImGui::InvisibleButton("##close", closeSize))
                {
                    ui.closedSidebarViews.insert(view->id);
                }
                ImGui::GetWindowDrawList()->AddText(closePos,
                                                    ImGui::ColorConvertFloat4ToU32(ImGui::IsItemHovered() ? Red : WithAlpha(Muted, 0.55f)),
                                                    "x");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("hide panel (:panel %s to bring it back)", view->id.c_str());
                }
                const ImVec2 rule = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddLine(ImVec2(rule.x, rule.y + 2.0f), ImVec2(rule.x + ImGui::GetContentRegionAvail().x, rule.y + 2.0f),
                                                    ImGui::ColorConvertFloat4ToU32(WithAlpha(Muted, 0.15f)), 1.0f);
                ImGui::Dummy(ImVec2(1.0f, 6.0f));
                const auto draw = view->draw;
                if (draw)
                {
                    draw(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
                }
                ImGui::Dummy(ImVec2(1.0f, 14.0f));
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        const float editorX = bodyX + outlineWidth + gap;
        const float editorWidth = std::max(1.0f, avail.x - outlineWidth - gap - sidebarWidth - sidebarGap);
        ImGui::SetCursorPos(ImVec2(editorX, bodyY));

        // Journal notes get their calendar and prompt above the page
        if (editor.Journal().IsJournalPath(document.relativePath))
        {
            const float pageWidth = editorSettings.pageWidth > 0 ? std::min(static_cast<float>(editorSettings.pageWidth), editorWidth) : editorWidth;
            const float inset = std::max(0.0f, (editorWidth - pageWidth) * 0.5f) + static_cast<float>(editorSettings.fontSize) * 1.2f;
            ImGui::BeginGroup();
            DrawJournalCompanion(context, document, editorWidth, inset, std::max(1.0f, pageWidth - 40.0f));
            ImGui::EndGroup();
        }

        Software::Slate::Live::LiveEditorSettings settings;
        settings.fontSize = static_cast<float>(editorSettings.fontSize);
        settings.lineGap = static_cast<float>(editorSettings.lineSpacing);
        settings.livePreview = editorSettings.livePreview;
        settings.headingScale = editorSettings.headingScale;
        settings.tabWidth = editorSettings.tabWidth;
        settings.indentWithTabs = editorSettings.indentWithTabs;
        settings.smartLists = editorSettings.autoListContinuation;
        settings.highlightLine = editorSettings.highlightCurrentLine;
        settings.readOnly = reading;
        settings.captureNavigation = paletteOpen;
        settings.acceptInput = handleInput;
        settings.pageWidth = static_cast<float>(editorSettings.pageWidth);
        settings.caretBlink = editorSettings.cursorBlink;
        settings.linkUnderline = editorSettings.linkUnderline;
        settings.scrollMotion = editorSettings.scrollMotion;

        Software::Slate::Live::LiveEditorHooks hooks;
        hooks.openLink = [this](const std::string& target, bool isWikiLink) {
            m_pendingLink = target;
            m_pendingLinkIsWiki = isWikiLink;
        };
        hooks.openTag = [this](const std::string& tag) { m_pendingTag = tag; };
        hooks.resolveImage = [this, &context](const std::string& target) { return ResolveImage(context, target); };

        const float editorHeight = std::max(1.0f, bodyY + bodyHeight - ImGui::GetCursorPosY());
        PushSlateScrollbarStyle(editorSettings.scrollbarStyle);
        editor.DrawEditor(workspace.Documents(), "LiveMarkdownEditor", ImVec2(editorWidth, editorHeight), settings, hooks,
                          context.frame.elapsedSeconds);
        PopSlateScrollbarStyle();

        if (paletteOpen && editor.IsTextFocused())
        {
            DrawSlashCommandPalette(context, slashCommands);
        }
        ImGui::SetCursorPos(ImVec2(bodyX, bodyY + bodyHeight));
    }

    void SlateEditorMode::DrawSlashCommandPalette(Software::Core::Runtime::AppContext& context,
                                                  const std::vector<Software::Slate::SlashCommandDefinition>& commands)
    {
        auto& editor = EditorContext(context);
        const auto& live = editor.Editor();
        const std::size_t maxVisible = 8;
        const std::size_t visibleCount = std::min(commands.size(), maxVisible);
        int first = 0;
        if (m_slashCommandIndex >= static_cast<int>(visibleCount))
        {
            first = m_slashCommandIndex - static_cast<int>(visibleCount) + 1;
        }

        float labelWidth = 0.0f;
        for (const auto& command : commands)
        {
            labelWidth = std::max(labelWidth, ImGui::CalcTextSize(command.id.c_str()).x);
        }
        const float rowHeight = ImGui::GetTextLineHeight() + 10.0f;
        const float width = std::clamp(labelWidth + 300.0f, 320.0f, 520.0f);
        const float height = rowHeight * static_cast<float>(visibleCount) + 34.0f;

        // Open below the caret, or above it when there is no room
        ImVec2 position = live.CaretScreenPos();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (position.y + height > viewport->Pos.y + viewport->Size.y - 60.0f)
        {
            position.y -= height + live.CaretHeight() + 10.0f;
        }
        position.x = std::min(position.x, viewport->Pos.x + viewport->Size.x - width - 16.0f);

        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, WithAlpha(Panel, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(Muted, 0.3f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::Begin("##SlateSlashCommandPalette", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoScrollbar);

        auto* drawList = ImGui::GetWindowDrawList();
        const ImVec2 windowMin = ImGui::GetWindowPos();
        drawList->AddText(ImVec2(windowMin.x + 14.0f, windowMin.y + 8.0f), ImGui::ColorConvertFloat4ToU32(Muted), "insert");
        const char* hint = "tab/enter  esc";
        const float hintWidth = ImGui::CalcTextSize(hint).x;
        drawList->AddText(ImVec2(windowMin.x + width - hintWidth - 14.0f, windowMin.y + 8.0f),
                          ImGui::ColorConvertFloat4ToU32(WithAlpha(Muted, 0.6f)), hint);

        for (std::size_t i = 0; i < visibleCount; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(first) + i;
            if (index >= commands.size())
            {
                break;
            }
            const auto& command = commands[index];
            const bool selected = static_cast<int>(index) == m_slashCommandIndex;
            const ImVec2 rowMin(windowMin.x + 6.0f, windowMin.y + 28.0f + static_cast<float>(i) * rowHeight);
            const ImVec2 rowMax(windowMin.x + width - 6.0f, rowMin.y + rowHeight - 2.0f);
            if (selected)
            {
                drawList->AddRectFilled(rowMin, rowMax, ImGui::ColorConvertFloat4ToU32(WithAlpha(Green, 0.12f)), 6.0f);
                drawList->AddRectFilled(rowMin, ImVec2(rowMin.x + 3.0f, rowMax.y), ImGui::ColorConvertFloat4ToU32(Green), 1.5f);
            }
            const float textY = rowMin.y + 5.0f;
            drawList->AddText(ImVec2(rowMin.x + 12.0f, textY), ImGui::ColorConvertFloat4ToU32(selected ? Green : Amber),
                              ("/" + command.id).c_str());
            drawList->PushClipRect(rowMin, rowMax, true);
            drawList->AddText(ImVec2(rowMin.x + 30.0f + labelWidth, textY), ImGui::ColorConvertFloat4ToU32(selected ? Primary : Muted),
                              command.description.c_str());
            drawList->PopClipRect();

            ImGui::SetCursorScreenPos(rowMin);
            ImGui::PushID(static_cast<int>(index));
            ImGui::InvisibleButton("##slash-row", ImVec2(rowMax.x - rowMin.x, rowMax.y - rowMin.y));
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x * ImGui::GetIO().MouseDelta.y != 0.0f)
            {
                m_slashCommandIndex = static_cast<int>(index);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                const auto selectedCommand = command;
                ImGui::PopID();
                ImGui::End();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(2);
                ApplySlashCommand(context, *WorkspaceContext(context).Documents().Active(), selectedCommand);
                return;
            }
            ImGui::PopID();
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void SlateEditorMode::DrawOutline(Software::Core::Runtime::AppContext& context)
    {
        auto& ui = UiState(context);
        auto& editor = EditorContext(context);
        ui.visibleHeadings = editor.ParseHeadings(WorkspaceContext(context).Documents());
        ui.navigation.SetCount(ui.visibleHeadings.size());
        const std::size_t currentHeading = CurrentHeadingIndex(ui.visibleHeadings, CurrentCaretLine(editor));

        const float width = CenteredColumnWidth(760.0f);
        SetCursorCenteredForWidth(width);
        ImGui::BeginGroup();
        ImGui::TextColored(Cyan, "outline");
        ImGui::Dummy(ImVec2(1.0f, 6.0f));
        if (ui.visibleHeadings.empty())
        {
            ImGui::TextColored(Muted, "no headings yet. start a line with # to add one");
        }
        for (std::size_t i = 0; i < ui.visibleHeadings.size(); ++i)
        {
            const auto& heading = ui.visibleHeadings[i];
            const bool selected = i == ui.navigation.Selected();
            const bool current = i == currentHeading;
            const std::string indent(static_cast<std::size_t>(std::max(0, heading.level - 1)) * 2, ' ');
            ImGui::TextColored(selected ? Green : (current ? Cyan : Primary), "%s %s%s", selected ? ">" : " ", indent.c_str(),
                               heading.title.c_str());
            ImGui::SameLine();
            ImGui::TextColored(Muted, "%zu", heading.line);
        }
        ImGui::EndGroup();
    }

    void SlateEditorMode::DrawOutlinePanel(Software::Core::Runtime::AppContext& context)
    {
        auto& editor = EditorContext(context);
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        ui.visibleHeadings = editor.ParseHeadings(workspace.Documents());
        const std::size_t currentHeading = CurrentHeadingIndex(ui.visibleHeadings, CurrentCaretLine(editor));

        ImGui::TextColored(Muted, "outline");
        ImGui::Dummy(ImVec2(1.0f, 4.0f));
        if (ui.visibleHeadings.empty())
        {
            ImGui::TextColored(WithAlpha(Muted, 0.7f), "no headings");
            return;
        }

        auto* drawList = ImGui::GetWindowDrawList();
        for (std::size_t i = 0; i < ui.visibleHeadings.size(); ++i)
        {
            const auto& heading = ui.visibleHeadings[i];
            const bool current = i == currentHeading;
            const float indent = static_cast<float>(std::max(0, heading.level - 1)) * 12.0f;
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float rowHeight = ImGui::GetTextLineHeight() + 6.0f;
            ImGui::PushID(static_cast<int>(i));
            const bool clicked = ImGui::InvisibleButton("##heading", ImVec2(ImGui::GetContentRegionAvail().x, rowHeight));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            if (current || hovered)
            {
                drawList->AddRectFilled(start, ImVec2(start.x + ImGui::GetContentRegionAvail().x, start.y + rowHeight),
                                        ImGui::ColorConvertFloat4ToU32(WithAlpha(current ? Cyan : Muted, current ? 0.10f : 0.08f)), 4.0f);
            }
            if (current)
            {
                drawList->AddRectFilled(start, ImVec2(start.x + 2.0f, start.y + rowHeight), ImGui::ColorConvertFloat4ToU32(Cyan));
            }
            drawList->PushClipRect(start, ImVec2(start.x + ImGui::GetContentRegionAvail().x, start.y + rowHeight), true);
            drawList->AddText(ImVec2(start.x + 10.0f + indent, start.y + 3.0f),
                              ImGui::ColorConvertFloat4ToU32(current ? Cyan : (heading.level <= 1 ? Primary : WithAlpha(Primary, 0.75f))),
                              heading.title.c_str());
            drawList->PopClipRect();
            if (clicked)
            {
                editor.JumpToLine(heading.line);
                ui.editorView = Software::Slate::SlateEditorView::Document;
            }
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

    void SlateEditorMode::OpenLinkTarget(Software::Core::Runtime::AppContext& context, const std::string& rawTarget, bool isWikiLink)
    {
        auto& workspace = WorkspaceContext(context);
        const std::string lower = Software::Slate::PathUtils::ToLower(rawTarget);
        if (lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0 || lower.rfind("mailto:", 0) == 0)
        {
#if defined(_WIN32)
            ::ShellExecuteW(nullptr, L"open", Software::Slate::fs::u8path(rawTarget).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
            SetStatus("opened " + rawTarget);
            return;
        }

        std::string target = rawTarget.substr(0, rawTarget.find('#'));
        for (std::size_t pos = target.find("%20"); pos != std::string::npos; pos = target.find("%20"))
        {
            target.replace(pos, 3, " ");
        }
        if (target.empty())
        {
            return;
        }

        const auto* document = workspace.Documents().Active();
        const Software::Slate::fs::path folder = document ? document->relativePath.parent_path() : Software::Slate::fs::path();

        if (isWikiLink)
        {
            // Wiki links match a note name anywhere, or a workspace relative path
            const std::string wanted = Software::Slate::PathUtils::ToLower(target);
            for (const auto& file : workspace.Workspace().MarkdownFiles())
            {
                const std::string stem = Software::Slate::PathUtils::ToLower(file.stem().u8string());
                std::string relative = Software::Slate::PathUtils::ToLower(file.generic_u8string());
                if (relative.size() > 3 && relative.compare(relative.size() - 3, 3, ".md") == 0)
                {
                    relative.resize(relative.size() - 3);
                }
                if (stem == wanted || relative == wanted)
                {
                    OpenDocument(file, context);
                    return;
                }
            }

            Software::Slate::fs::path created;
            std::string error;
            if (!workspace.CreateNewNote(folder.empty() ? Software::Slate::fs::path(".") : folder, target, &created, &error))
            {
                SetError(error);
                return;
            }
            OpenDocument(created, context);
            SetStatus("created " + created.generic_u8string());
            return;
        }

        const Software::Slate::fs::path relative = (folder / Software::Slate::fs::u8path(target)).lexically_normal();
        const Software::Slate::fs::path absolute = workspace.Workspace().Resolve(relative);
        std::error_code ec;
        if (Software::Slate::fs::exists(absolute, ec))
        {
            if (Software::Slate::PathUtils::ToLower(relative.extension().u8string()) == ".md")
            {
                OpenDocument(relative, context);
            }
            else
            {
#if defined(_WIN32)
                ::ShellExecuteW(nullptr, L"open", absolute.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
            }
            return;
        }
        SetError("link target not found: " + target);
    }

    std::vector<Software::Slate::SlashCommandDefinition> SlateEditorMode::ExtensionSlashCommands(
        Software::Core::Runtime::AppContext& context) const
    {
        std::vector<Software::Slate::SlashCommandDefinition> commands;
        if (const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>())
        {
            for (const auto& command : registry->SlashCommands())
            {
                commands.push_back({command.id, command.label, command.description,
                                    Software::Slate::SlashCommandKind::Extension});
            }
        }
        return commands;
    }

    bool SlateEditorMode::ApplySlashCommand(Software::Core::Runtime::AppContext& context,
                                            Software::Slate::DocumentService::Document& document,
                                            const Software::Slate::SlashCommandDefinition& command)
    {
        const Software::Slate::SlashCommandKind kind = command.kind;
        auto& workspace = WorkspaceContext(context);
        auto& editor = EditorContext(context);
        const auto& editorSettings = workspace.CurrentEditorSettings();

        std::string activeLine;
        std::size_t caretColumn = 0;
        std::string activeQuery;
        std::size_t slashTokenStart = 0;
        std::size_t slashTokenEnd = 0;
        const bool hasSlashToken = editor.ActiveLineTextAndCaret(&activeLine, &caretColumn) &&
                                   Software::Slate::SlashCommandService::QueryFromLineAtCaret(
                                       activeLine, caretColumn, &activeQuery, &slashTokenStart, &slashTokenEnd);
        auto replacementLine = [&](const std::string& replacement) {
            if (!hasSlashToken)
            {
                return replacement;
            }
            return activeLine.substr(0, slashTokenStart) + replacement + activeLine.substr(slashTokenEnd);
        };

        m_slashCommandQuery.clear();
        m_slashCommandSuppressed = false;
        m_slashCommandSuppressedQuery.clear();
        m_slashCommandIndex = 0;

        if (kind == Software::Slate::SlashCommandKind::Todo)
        {
            if (editor.ReplaceActiveLineWithText(workspace.Documents(), replacementLine("/todo"), context.frame.elapsedSeconds))
            {
                BeginTodoCreate(context, true);
                return true;
            }
            return false;
        }

        if (kind == Software::Slate::SlashCommandKind::Image)
        {
            std::string markdown;
            std::string error;
            if (editorSettings.pasteClipboardImages && workspace.Assets().HasClipboardImage() &&
                workspace.Assets().CreateMarkdownImageFromClipboard(
                    document.relativePath,
                    static_cast<Software::Slate::ImageStoragePolicy>(editorSettings.imageStoragePolicy),
                    &markdown,
                    &error))
            {
                if (editor.ReplaceActiveLineWithText(workspace.Documents(), replacementLine(markdown), context.frame.elapsedSeconds))
                {
                    SetStatus("pasted image");
                    return true;
                }
            }
            else if (!error.empty())
            {
                SetError(error);
            }
        }

        if (kind == Software::Slate::SlashCommandKind::Extension)
        {
            const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>();
            const auto* extension = registry ? registry->FindSlashCommand(command.id) : nullptr;
            if (!extension || !extension->run)
            {
                return false;
            }

            // Copy first: the plugin may re-register its commands while running
            const auto run = extension->run;
            std::string replacement;
            std::string error;
            if (!run(&replacement, &error))
            {
                SetError(error);
                return false;
            }

            std::string normalized;
            for (const char ch : replacement)
            {
                if (ch == '\n')
                {
                    normalized += document.lineEnding;
                }
                else if (ch != '\r')
                {
                    normalized.push_back(ch);
                }
            }
            return editor.ReplaceActiveLineWithText(workspace.Documents(), replacementLine(normalized), context.frame.elapsedSeconds);
        }

        const std::string replacement = Software::Slate::SlashCommandService::BuildMarkdownTemplate(kind, document.lineEnding);
        if (replacement.empty())
        {
            return false;
        }
        const bool replaced = editor.ReplaceActiveLineWithText(workspace.Documents(), replacementLine(replacement), context.frame.elapsedSeconds);
        if (replaced)
        {
            switch (kind)
            {
            case Software::Slate::SlashCommandKind::Table:
                SetStatus("inserted table");
                break;
            case Software::Slate::SlashCommandKind::Callout:
                SetStatus("inserted callout");
                break;
            case Software::Slate::SlashCommandKind::Image:
                SetStatus("inserted image placeholder");
                break;
            case Software::Slate::SlashCommandKind::CodeBlock:
                SetStatus("inserted code block");
                break;
            case Software::Slate::SlashCommandKind::Checklist:
                SetStatus("inserted checklist item");
                break;
            case Software::Slate::SlashCommandKind::Quote:
                SetStatus("inserted quote");
                break;
            case Software::Slate::SlashCommandKind::Divider:
                SetStatus("inserted divider");
                break;
            case Software::Slate::SlashCommandKind::Link:
                SetStatus("inserted link");
                break;
            case Software::Slate::SlashCommandKind::Todo:
            case Software::Slate::SlashCommandKind::Extension:
                break;
            }
        }
        return replaced;
    }
}
