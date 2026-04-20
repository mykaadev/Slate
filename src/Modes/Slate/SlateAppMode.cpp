#include "Modes/Slate/SlateAppMode.h"

#include "App/Slate/PathUtils.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <sstream>
#include <string_view>
#include <utility>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        constexpr std::size_t InvalidLine = static_cast<std::size_t>(-1);

        std::vector<std::string> EditorLinesFromText(const std::string& text)
        {
            auto lines = Software::Slate::MarkdownService::SplitLines(text);
            if (lines.empty())
            {
                lines.emplace_back();
            }
            return lines;
        }

        std::string JoinEditorLines(const std::vector<std::string>& lines, const std::string& lineEnding)
        {
            std::string text;
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                if (i > 0)
                {
                    text += lineEnding;
                }
                text += lines[i];
            }
            return text;
        }

        std::size_t LeadingSpaceCount(const std::string& line)
        {
            std::size_t count = 0;
            while (count < line.size() && line[count] == ' ')
            {
                ++count;
            }
            return count;
        }

        bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
        }

        std::string InlineMarkdownText(std::string_view text)
        {
            std::string out;
            for (std::size_t i = 0; i < text.size();)
            {
                if (StartsWith(text.substr(i), "!["))
                {
                    const std::size_t closeLabel = text.find(']', i + 2);
                    if (closeLabel != std::string_view::npos && closeLabel + 1 < text.size() &&
                        text[closeLabel + 1] == '(')
                    {
                        const std::size_t closeTarget = text.find(')', closeLabel + 2);
                        if (closeTarget != std::string_view::npos)
                        {
                            const std::string_view alt = text.substr(i + 2, closeLabel - i - 2);
                            const std::string_view target = text.substr(closeLabel + 2, closeTarget - closeLabel - 2);
                            out += "[image";
                            if (!alt.empty())
                            {
                                out += ": ";
                                out += alt;
                            }
                            else if (!target.empty())
                            {
                                out += ": ";
                                out += Software::Slate::fs::path(std::string(target)).filename().string();
                            }
                            out += "]";
                            i = closeTarget + 1;
                            continue;
                        }
                    }
                }

                if (StartsWith(text.substr(i), "[["))
                {
                    const std::size_t close = text.find("]]", i + 2);
                    if (close != std::string_view::npos)
                    {
                        std::string inner(text.substr(i + 2, close - i - 2));
                        const std::size_t pipe = inner.find('|');
                        out += pipe == std::string::npos ? inner : inner.substr(pipe + 1);
                        i = close + 2;
                        continue;
                    }
                }

                if (text[i] == '[')
                {
                    const std::size_t closeLabel = text.find(']', i + 1);
                    if (closeLabel != std::string_view::npos && closeLabel + 1 < text.size() &&
                        text[closeLabel + 1] == '(')
                    {
                        const std::size_t closeTarget = text.find(')', closeLabel + 2);
                        if (closeTarget != std::string_view::npos)
                        {
                            out += text.substr(i + 1, closeLabel - i - 1);
                            i = closeTarget + 1;
                            continue;
                        }
                    }
                }

                if (StartsWith(text.substr(i), "**") || StartsWith(text.substr(i), "__"))
                {
                    const std::string_view marker = text.substr(i, 2);
                    const std::size_t close = text.find(marker, i + 2);
                    if (close != std::string_view::npos)
                    {
                        out += text.substr(i + 2, close - i - 2);
                        i = close + 2;
                        continue;
                    }
                }

                if (text[i] == '`')
                {
                    const std::size_t close = text.find('`', i + 1);
                    if (close != std::string_view::npos)
                    {
                        out += text.substr(i + 1, close - i - 1);
                        i = close + 1;
                        continue;
                    }
                }

                if ((text[i] == '*' || text[i] == '_') && i + 1 < text.size() && text[i + 1] != text[i])
                {
                    const char marker = text[i];
                    const std::size_t close = text.find(marker, i + 1);
                    if (close != std::string_view::npos)
                    {
                        out += text.substr(i + 1, close - i - 1);
                        i = close + 1;
                        continue;
                    }
                }

                out.push_back(text[i]);
                ++i;
            }
            return out;
        }

        void DrawScaledText(const ImVec4& color, float scale, const char* text)
        {
            ImGui::SetWindowFontScale(scale);
            ImGui::TextColored(color, "%s", text);
            ImGui::SetWindowFontScale(1.0f);
        }
    }

    void SlateAppMode::OnEnter(Software::Core::Runtime::AppContext& context)
    {
        (void)context;
        std::string error;
        if (!m_workspaceRegistry.Load(&error))
        {
            SetError(error);
        }

        if (const auto* vault = m_workspaceRegistry.ActiveVault())
        {
            OpenVault(*vault);
            m_screen = Screen::Home;
        }
        else
        {
            m_screen = Screen::WorkspaceSetup;
            SetStatus("create a workspace to begin");
        }
    }

    void SlateAppMode::Update(Software::Core::Runtime::AppContext& context)
    {
        if (context.frame.elapsedSeconds - m_lastScanSeconds > 2.0)
        {
            m_documents.CheckExternalChange();
            m_lastScanSeconds = context.frame.elapsedSeconds;
        }

        std::string error;
        if (!m_documents.SaveIfNeeded(context.frame.elapsedSeconds, &error))
        {
            SetError(error);
        }

        if (context.frame.elapsedSeconds - m_lastIndexSeconds > 5.0)
        {
            m_search.Rebuild(m_workspace);
            m_lastIndexSeconds = context.frame.elapsedSeconds;
        }
    }

    void SlateAppMode::DrawUI(Software::Core::Runtime::AppContext& context)
    {
        ProcessDroppedFiles(context);
        HandleGlobalKeys(context);

        DrawRootBegin();

        if (m_screen == Screen::Prompt)
        {
            DrawScreenContent(m_returnScreen, context, false);
            DrawPromptOverlay();
        }
        else if (m_screen == Screen::Confirm)
        {
            DrawScreenContent(m_returnScreen, context, false);
            DrawConfirm(context);
        }
        else
        {
            DrawScreenContent(m_screen, context, !m_searchOverlayOpen);
        }

        if (m_searchOverlayOpen)
        {
            DrawSearchOverlay();
        }

        DrawStatusLine();
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(5);
    }

    void SlateAppMode::OpenDocument(const Software::Slate::fs::path& relativePath)
    {
        if (m_documents.HasOpenDocument())
        {
            SaveActiveDocument();
        }

        std::string error;
        if (!m_documents.Open(relativePath, &error))
        {
            SetError(error);
            return;
        }

        if (auto* document = m_documents.Active())
        {
            (void)document;
            m_editorTextFocused = true;
            m_focusEditor = true;
            m_editorCursorPos = 0;
            m_editorRequestedCursorPos = 0;
            m_editorActiveLine = 0;
            m_editorScratchLine = InvalidLine;
            m_editorLineText.clear();
        }

        m_workspace.TouchRecent(relativePath);
        m_screen = Screen::Editor;
        m_returnScreen = Screen::Home;
        m_folderPickerActive = false;
        CloseSearchOverlay();
        SetStatus("opened " + relativePath.generic_string());

        const auto* document = m_documents.Active();
        if (document && document->recoveryAvailable)
        {
            BeginConfirm(ConfirmAction::RestoreRecovery,
                         "recovery is newer than disk. y restore / n discard / esc later");
        }
    }

    void SlateAppMode::OpenTodayJournal()
    {
        Software::Slate::fs::path daily;
        std::string error;
        if (!m_workspace.EnsureDailyNote(&daily, &error))
        {
            SetError(error);
            return;
        }
        m_search.Rebuild(m_workspace);
        OpenDocument(daily);
    }

    void SlateAppMode::CreateNewNote(const std::string& value)
    {
        const Software::Slate::fs::path target = m_workspace.MakeCollisionSafeMarkdownPath(m_pendingFolderPath, value);
        Software::Slate::fs::path created;
        std::string error;
        if (!m_workspace.CreateMarkdownFile(target, "# " + target.stem().string() + "\n\n", &created, &error))
        {
            SetError(error);
            m_screen = m_returnScreen;
            return;
        }
        m_search.Rebuild(m_workspace);
        m_folderPickerActive = false;
        OpenDocument(created);
    }

    void SlateAppMode::BeginFolderPicker()
    {
        m_pendingFolderPath = ".";
        m_folderPickerActive = true;
        m_screen = Screen::FileTree;
        std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
        m_filterActive = false;
        m_navigation.Reset();
        SetStatus("choose a folder for the new note");
    }

    void SlateAppMode::BeginSearchOverlay(bool clearQuery)
    {
        if (clearQuery)
        {
            std::fill(m_searchBuffer.begin(), m_searchBuffer.end(), '\0');
            m_searchNavigation.Reset();
        }
        m_searchOverlayOpen = true;
        m_focusFilter = true;
    }

    void SlateAppMode::CloseSearchOverlay()
    {
        m_searchOverlayOpen = false;
    }

    void SlateAppMode::BeginPrompt(PromptAction action, const std::string& title, const std::string& initialValue)
    {
        m_promptAction = action;
        m_promptTitle = title;
        std::fill(m_promptBuffer.begin(), m_promptBuffer.end(), '\0');
        std::snprintf(m_promptBuffer.data(), m_promptBuffer.size(), "%s", initialValue.c_str());
        m_returnScreen = m_screen == Screen::Prompt ? m_returnScreen : m_screen;
        m_screen = Screen::Prompt;
        m_focusPrompt = true;
    }

    void SlateAppMode::ExecutePrompt()
    {
        const std::string value = Software::Slate::PathUtils::Trim(m_promptBuffer.data());
        const PromptAction action = m_promptAction;
        m_promptAction = PromptAction::None;

        switch (action)
        {
        case PromptAction::NewNote:
            BeginFolderPicker();
            break;
        case PromptAction::NewNoteName:
            CreateNewNote(value);
            break;
        case PromptAction::Search:
            std::snprintf(m_searchBuffer.data(), m_searchBuffer.size(), "%s", value.c_str());
            m_screen = m_returnScreen;
            BeginSearchOverlay(false);
            break;
        case PromptAction::Workspace:
            OpenWorkspace(value);
            m_screen = Screen::Home;
            break;
        case PromptAction::WorkspaceTitle:
            m_pendingWorkspaceTitle = value.empty() ? "My Workspace" : value;
            BeginPrompt(PromptAction::WorkspaceEmoji, "workspace emoji", Software::Slate::WorkspaceRegistryService::DefaultEmoji());
            break;
        case PromptAction::WorkspaceEmoji:
        {
            m_pendingWorkspaceEmoji = value.empty() ? Software::Slate::WorkspaceRegistryService::DefaultEmoji() : value;
            const auto defaultPath =
                Software::Slate::WorkspaceRegistryService::DefaultWorkspacePathForTitle(m_pendingWorkspaceTitle);
            BeginPrompt(PromptAction::WorkspacePath, "workspace location", defaultPath.string());
            break;
        }
        case PromptAction::WorkspacePath:
        {
            Software::Slate::fs::path path = value.empty()
                                                ? Software::Slate::WorkspaceRegistryService::DefaultWorkspacePathForTitle(
                                                      m_pendingWorkspaceTitle)
                                                : Software::Slate::fs::path(value);
            std::error_code ec;
            Software::Slate::fs::create_directories(path, ec);
            if (ec)
            {
                SetError(ec.message());
                m_screen = Screen::WorkspaceSetup;
                break;
            }

            const auto vault = m_workspaceRegistry.AddVault(m_pendingWorkspaceEmoji, m_pendingWorkspaceTitle, path);
            std::string error;
            if (!m_workspaceRegistry.Save(&error))
            {
                SetError(error);
            }
            OpenVault(vault);
            m_screen = Screen::Home;
            break;
        }
        case PromptAction::RenameMove:
        {
            m_pendingTargetPath = Software::Slate::PathUtils::NormalizeRelative(value);
            m_pendingRewritePlan = m_links.BuildRenamePlan(m_workspace, m_pendingPath, m_pendingTargetPath);
            if (m_pendingRewritePlan.TotalReplacements() > 0)
            {
                BeginConfirm(ConfirmAction::ApplyRenameMove,
                             "rename/move updates " + std::to_string(m_pendingRewritePlan.TotalReplacements()) +
                                 " links. y apply / esc cancel");
            }
            else
            {
                std::string error;
                if (!m_workspace.RenameOrMove(m_pendingPath, m_pendingTargetPath, &error))
                {
                    SetError(error);
                }
                else
                {
                    SetStatus("moved " + m_pendingPath.generic_string() + " -> " +
                              m_pendingTargetPath.generic_string());
                    m_search.Rebuild(m_workspace);
                }
                m_screen = Screen::FileTree;
            }
            break;
        }
        case PromptAction::Command:
            ExecuteCommand(value);
            break;
        case PromptAction::None:
            m_screen = m_returnScreen;
            break;
        }
    }

    void SlateAppMode::ExecuteCommand(const std::string& command)
    {
        const std::string trimmed = Software::Slate::PathUtils::Trim(command);
        if (trimmed == "q")
        {
            BeginConfirm(ConfirmAction::Quit, "quit Slate? y quit / esc cancel");
        }
        else if (trimmed == "w")
        {
            SaveActiveDocument();
            m_screen = m_returnScreen;
        }
        else if (trimmed == "wq")
        {
            SaveActiveDocument();
            BeginConfirm(ConfirmAction::Quit, "quit Slate? y quit / esc cancel");
        }
        else if (trimmed == "today" || trimmed == "j")
        {
            OpenTodayJournal();
        }
        else if (trimmed.rfind("open ", 0) == 0)
        {
            OpenDocument(trimmed.substr(5));
        }
        else if (trimmed == "new" || trimmed.rfind("new ", 0) == 0)
        {
            BeginFolderPicker();
        }
        else if (trimmed == "workspaces" || trimmed == "vaults")
        {
            m_workspaceNavigation.SetCount(m_workspaceRegistry.Vaults().size());
            m_screen = Screen::WorkspaceSwitcher;
        }
        else if (trimmed == "new workspace" || trimmed == "new vault")
        {
            BeginWorkspaceCreate();
        }
        else if (trimmed.rfind("workspace ", 0) == 0)
        {
            OpenWorkspace(trimmed.substr(10));
            m_screen = Screen::Home;
        }
        else if (trimmed.rfind("search ", 0) == 0)
        {
            std::snprintf(m_searchBuffer.data(), m_searchBuffer.size(), "%s", trimmed.substr(7).c_str());
            m_screen = m_returnScreen;
            BeginSearchOverlay(false);
        }
        else
        {
            SetError("unknown command: " + trimmed);
            m_screen = m_returnScreen;
        }
    }

    void SlateAppMode::BeginConfirm(ConfirmAction action, const std::string& message)
    {
        m_confirmAction = action;
        m_confirmMessage = message;
        if (m_screen != Screen::Prompt && m_screen != Screen::Confirm)
        {
            m_returnScreen = m_screen;
        }
        m_screen = Screen::Confirm;
    }

    void SlateAppMode::ExecuteConfirm(bool accepted, Software::Core::Runtime::AppContext& context)
    {
        const ConfirmAction action = m_confirmAction;
        m_confirmAction = ConfirmAction::None;

        if (!accepted)
        {
            if (action == ConfirmAction::RestoreRecovery)
            {
                std::string error;
                if (m_documents.DiscardRecovery(&error))
                {
                    SetStatus("recovery discarded");
                }
                else
                {
                    SetError(error);
                }
            }
            m_screen = m_returnScreen;
            return;
        }

        std::string error;
        switch (action)
        {
        case ConfirmAction::DeletePath:
            if (m_workspace.DeletePath(m_pendingPath, &error))
            {
                SetStatus("deleted " + m_pendingPath.generic_string());
                m_search.Rebuild(m_workspace);
                if (m_documents.Active() && m_documents.Active()->relativePath == m_pendingPath)
                {
                    m_documents.Close();
                    m_editorTextFocused = false;
                    m_focusEditor = false;
                    m_editorScratchLine = InvalidLine;
                    m_editorLineText.clear();
                }
            }
            else
            {
                SetError(error);
            }
            m_screen = Screen::FileTree;
            break;
        case ConfirmAction::ApplyRenameMove:
            if (!m_links.ApplyRewritePlan(m_workspace, m_pendingRewritePlan, &error))
            {
                SetError(error);
                m_screen = Screen::FileTree;
                break;
            }
            if (!m_workspace.RenameOrMove(m_pendingPath, m_pendingTargetPath, &error))
            {
                SetError(error);
            }
            else
            {
                SetStatus("moved " + m_pendingPath.generic_string() + " -> " +
                          m_pendingTargetPath.generic_string());
                m_search.Rebuild(m_workspace);
            }
            m_screen = Screen::FileTree;
            break;
        case ConfirmAction::RestoreRecovery:
            if (m_documents.RestoreRecovery(&error))
            {
                if (auto* document = m_documents.Active())
                {
                    (void)document;
                    m_editorTextFocused = true;
                    m_focusEditor = true;
                    m_editorActiveLine = 0;
                    m_editorScratchLine = InvalidLine;
                    m_editorRequestedCursorPos = 0;
                }
                SetStatus("recovery restored");
            }
            else
            {
                SetError(error);
            }
            m_screen = Screen::Editor;
            break;
        case ConfirmAction::DiscardRecovery:
            m_documents.DiscardRecovery(&error);
            m_screen = Screen::Editor;
            break;
        case ConfirmAction::Quit:
            SaveActiveDocument();
            context.quitRequested = true;
            break;
        case ConfirmAction::None:
            m_screen = m_returnScreen;
            break;
        }
    }

    void SlateAppMode::BeginRenameMove(const Software::Slate::fs::path& relativePath)
    {
        m_pendingPath = relativePath;
        BeginPrompt(PromptAction::RenameMove, "move / rename path", relativePath.generic_string());
    }

    void SlateAppMode::BeginDelete(const Software::Slate::fs::path& relativePath)
    {
        m_pendingPath = relativePath;
        BeginConfirm(ConfirmAction::DeletePath, "delete " + relativePath.generic_string() + "? y delete / esc cancel");
    }

    void SlateAppMode::PrepareList(Screen screen)
    {
        m_visiblePaths.clear();
        const std::string filter(m_filterBuffer.data());

        if (screen == Screen::Recent)
        {
            for (const auto& path : m_workspace.RecentFiles())
            {
                if (ContainsFilter(path.generic_string(), filter.c_str()))
                {
                    m_visiblePaths.push_back(path);
                }
            }
        }
        else
        {
            for (const auto& path : m_workspace.MarkdownFiles())
            {
                const bool journal = Software::Slate::PathUtils::ToLower(path.generic_string()).rfind("journal/", 0) == 0;
                const bool docsScreen = screen == Screen::DocsIndex;
                if (docsScreen && journal)
                {
                    continue;
                }
                if (ContainsFilter(path.generic_string(), filter.c_str()))
                {
                    m_visiblePaths.push_back(path);
                }
            }
        }

        m_navigation.SetCount(m_visiblePaths.size());
    }

    void SlateAppMode::PrepareTreeRows(bool folderPicker)
    {
        m_treeRows.clear();

        if (folderPicker)
        {
            Software::Slate::TreeViewRow root;
            root.relativePath = ".";
            root.isDirectory = true;
            root.depth = 0;
            root.selectable = true;
            root.matchedFilter = false;
            m_treeRows.push_back(root);

            auto rows = Software::Slate::BuildTreeRows(m_workspace.Entries(), m_collapsedFolders, m_filterBuffer.data(),
                                                       Software::Slate::TreeViewMode::Folders);
            for (auto& row : rows)
            {
                row.depth += 1;
                row.selectable = true;
                m_treeRows.push_back(std::move(row));
            }
        }
        else
        {
            m_treeRows = Software::Slate::BuildTreeRows(m_workspace.Entries(), m_collapsedFolders, m_filterBuffer.data(),
                                                        Software::Slate::TreeViewMode::Files);
        }

        m_navigation.SetCount(m_treeRows.size());
    }

    void SlateAppMode::OpenSelectedPath()
    {
        if (!m_navigation.HasSelection() || m_visiblePaths.empty())
        {
            return;
        }
        OpenDocument(m_visiblePaths[m_navigation.Selected()]);
    }

    void SlateAppMode::ActivateSelectedTreeRow()
    {
        if (!m_navigation.HasSelection() || m_treeRows.empty())
        {
            return;
        }

        const auto& row = m_treeRows[m_navigation.Selected()];
        if (m_folderPickerActive)
        {
            if (row.isDirectory)
            {
                m_pendingFolderPath = row.relativePath;
                BeginPrompt(PromptAction::NewNoteName, "new note name", "Untitled.md");
            }
            return;
        }

        if (row.isDirectory)
        {
            ToggleSelectedFolder(m_collapsedFolders.find(Software::Slate::TreePathKey(row.relativePath)) !=
                                 m_collapsedFolders.end());
        }
        else
        {
            OpenDocument(row.relativePath);
        }
    }

    void SlateAppMode::ToggleSelectedFolder(bool expanded)
    {
        if (!m_navigation.HasSelection() || m_treeRows.empty())
        {
            return;
        }
        const auto& row = m_treeRows[m_navigation.Selected()];
        if (!row.isDirectory || row.relativePath == ".")
        {
            return;
        }

        const std::string key = Software::Slate::TreePathKey(row.relativePath);
        if (expanded)
        {
            m_collapsedFolders.erase(key);
        }
        else
        {
            m_collapsedFolders.insert(key);
        }
    }

    void SlateAppMode::SaveActiveDocument()
    {
        std::string error;
        if (m_documents.Save(&error))
        {
            SetStatus("saved");
            m_workspace.Scan(nullptr);
            m_search.Rebuild(m_workspace);
        }
        else
        {
            SetError(error);
        }
    }

    void SlateAppMode::ProcessDroppedFiles(Software::Core::Runtime::AppContext& context)
    {
        if (!context.droppedFiles || context.droppedFiles->empty())
        {
            return;
        }

        if (!m_documents.HasOpenDocument())
        {
            SetError("open a note before dropping image files");
            context.droppedFiles->clear();
            return;
        }

        auto* document = m_documents.Active();
        int inserted = 0;
        for (const auto& pathText : *context.droppedFiles)
        {
            Software::Slate::fs::path assetRelative;
            std::string error;
            if (m_assets.CopyImageAsset(document->relativePath, pathText, &assetRelative, &error))
            {
                InsertTextAtEditorCursor(Software::Slate::AssetService::MarkdownImageLink(document->relativePath,
                                                                                         assetRelative) +
                                             document->lineEnding,
                                         context.frame.elapsedSeconds);
                ++inserted;
            }
            else
            {
                SetError(error);
            }
        }

        if (inserted > 0)
        {
            SetStatus("inserted " + std::to_string(inserted) + " image link(s)");
        }
        context.droppedFiles->clear();
    }

    void SlateAppMode::DrawRootBegin()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Background);
        ImGui::PushStyleColor(ImGuiCol_Text, Primary);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Panel);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
        ImGui::Begin("SlateRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    }

    void SlateAppMode::DrawScreenContent(Screen screen, Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        switch (screen)
        {
        case Screen::WorkspaceSetup:
            if (handleInput)
            {
                HandleWorkspaceKeys();
            }
            DrawWorkspaceSetup();
            break;
        case Screen::WorkspaceSwitcher:
            if (handleInput)
            {
                HandleWorkspaceKeys();
            }
            DrawWorkspaceSwitcher();
            break;
        case Screen::Home:
            if (handleInput)
            {
                HandleHomeKeys(context);
            }
            DrawHome(context);
            break;
        case Screen::Editor:
            if (handleInput)
            {
                HandleEditorKeys(context);
            }
            DrawEditor(context);
            break;
        case Screen::QuickOpen:
        case Screen::Recent:
        case Screen::DocsIndex:
            PrepareList(screen);
            if (handleInput)
            {
                HandleListKeys();
            }
            DrawPathList(screen == Screen::QuickOpen ? "quick open"
                                                     : (screen == Screen::Recent ? "recent files" : "gdd / docs"),
                         "no matching files");
            break;
        case Screen::FileTree:
            PrepareTreeRows(m_folderPickerActive);
            if (handleInput)
            {
                HandleTreeKeys(m_folderPickerActive);
            }
            DrawFileTree(m_folderPickerActive);
            break;
        case Screen::Outline:
            if (handleInput)
            {
                HandleListKeys();
            }
            DrawOutline();
            break;
        case Screen::Help:
            DrawHelp();
            break;
        case Screen::Prompt:
        case Screen::Confirm:
            break;
        }
    }

    void SlateAppMode::DrawHome(Software::Core::Runtime::AppContext& context)
    {
        (void)context;
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.12f));
        TextCentered("Slate", Cyan);
        ImGui::Spacing();
        if (const auto* vault = m_workspaceRegistry.ActiveVault())
        {
            const std::string title = vault->emoji + " " + vault->title;
            TextCentered(title.c_str(), Muted);
        }
        else
        {
            TextCentered(m_workspace.Root().filename().string().c_str(), Muted);
        }
        ImGui::Spacing();
        TextCentered("journal", Primary);

        ImGui::Dummy(ImVec2(1.0f, 28.0f));
        const float columnWidth = 300.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - columnWidth) * 0.5f);
        ImGui::BeginGroup();
        TextLine("j", "today journal");
        TextLine("n", "new note");
        TextLine("o", "quick open");
        TextLine("s", "search");
        TextLine("r", "recent files");
        TextLine("f", "file tree");
        TextLine("g", "gdd / docs");
        TextLine("w", "workspaces");
        TextLine(":", "command");
        TextLine("?", "shortcuts");
        TextLine("q", "quit");
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const std::string stat = "markdown files: " + std::to_string(m_workspace.MarkdownFiles().size()) +
                                 "   recent: " + std::to_string(m_workspace.RecentFiles().size());
        TextCentered(stat.c_str(), Green);
    }

    void SlateAppMode::DrawEditor(Software::Core::Runtime::AppContext& context)
    {
        auto* document = m_documents.Active();
        if (!document)
        {
            m_screen = Screen::Home;
            return;
        }

        ImGui::TextColored(Cyan, "%s", document->relativePath.generic_string().c_str());
        ImGui::SameLine();
        ImGui::TextColored(document->conflict ? Red : (document->dirty ? Amber : Green), "%s",
                           document->conflict ? "external change" : (document->dirty ? "dirty" : "saved"));
        ImGui::Separator();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("LiveMarkdownEditorPage", ImVec2(0.0f, avail.y - 58.0f), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        DrawLiveMarkdownEditor(*document, context);
        ImGui::EndChild();
    }

    void SlateAppMode::DrawLiveMarkdownEditor(Software::Slate::DocumentService::Document& document,
                                              Software::Core::Runtime::AppContext& context)
    {
        auto lines = EditorLinesFromText(document.text);
        if (m_editorActiveLine >= lines.size())
        {
            SetEditorActiveLine(lines.empty() ? 0 : lines.size() - 1, 0);
        }

        if (m_editorScratchLine != m_editorActiveLine)
        {
            m_editorLineText = lines[m_editorActiveLine];
            m_editorScratchLine = m_editorActiveLine;
        }

        bool inCodeFence = false;
        bool inFrontmatter = !lines.empty() && Software::Slate::PathUtils::Trim(lines.front()) == "---";
        bool checkingFrontmatter = inFrontmatter;

        const float pageWidth = std::min(ImGui::GetContentRegionAvail().x, 980.0f);
        const float pageStartX = std::max(0.0f, (ImGui::GetContentRegionAvail().x - pageWidth) * 0.5f);
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
            if (i == m_editorActiveLine)
            {
                if (m_focusEditor)
                {
                    ImGui::SetKeyboardFocusHere();
                    m_focusEditor = false;
                }

                ImGui::SetNextItemWidth(pageWidth);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.095f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, Primary);
                const TextInputResult result = InputTextString("##LiveMarkdownLine", m_editorLineText,
                                                               ImGuiInputTextFlags_EnterReturnsTrue,
                                                               &m_editorCursorPos,
                                                               m_editorRequestedCursorPos);
                m_editorRequestedCursorPos = -1;
                m_editorTextFocused = ImGui::IsItemActive();
                ImGui::PopStyleColor(2);

                if (result.edited)
                {
                    ReplaceEditorLine(document, lines, i, m_editorLineText, context.frame.elapsedSeconds);
                }

                if (result.submitted)
                {
                    SplitEditorLine(document, lines, context.frame.elapsedSeconds);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && IsKeyPressed(ImGuiKey_Backspace) && m_editorCursorPos <= 0 &&
                    m_editorActiveLine > 0)
                {
                    MergeEditorLineWithPrevious(document, lines, context.frame.elapsedSeconds);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && IsKeyPressed(ImGuiKey_UpArrow) && m_editorActiveLine > 0)
                {
                    ReplaceEditorLine(document, lines, i, m_editorLineText, context.frame.elapsedSeconds);
                    SetEditorActiveLine(m_editorActiveLine - 1, m_editorCursorPos);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && IsKeyPressed(ImGuiKey_DownArrow) && m_editorActiveLine + 1 < lines.size())
                {
                    ReplaceEditorLine(document, lines, i, m_editorLineText, context.frame.elapsedSeconds);
                    SetEditorActiveLine(m_editorActiveLine + 1, m_editorCursorPos);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }
            }
            else
            {
                DrawMarkdownLine(lines[i], inCodeFence, lineIsFrontmatter);
            }

            ImGui::EndGroup();
            if (ImGui::IsItemClicked())
            {
                SetEditorActiveLine(i, static_cast<int>(lines[i].size()));
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
    }

    void SlateAppMode::SetEditorActiveLine(std::size_t line, int requestedCursor)
    {
        m_editorActiveLine = line;
        m_editorScratchLine = InvalidLine;
        m_editorRequestedCursorPos = requestedCursor;
        m_focusEditor = true;
        m_editorTextFocused = true;
    }

    void SlateAppMode::ReplaceEditorLine(Software::Slate::DocumentService::Document& document,
                                         std::vector<std::string>& lines,
                                         std::size_t line,
                                         const std::string& text,
                                         double elapsedSeconds)
    {
        if (line >= lines.size() || lines[line] == text)
        {
            return;
        }

        lines[line] = text;
        document.text = JoinEditorLines(lines, document.lineEnding);
        m_documents.MarkEdited(elapsedSeconds);
    }

    void SlateAppMode::SplitEditorLine(Software::Slate::DocumentService::Document& document,
                                       std::vector<std::string>& lines,
                                       double elapsedSeconds)
    {
        if (m_editorActiveLine >= lines.size())
        {
            return;
        }

        const int cursor = std::clamp(m_editorCursorPos, 0, static_cast<int>(m_editorLineText.size()));
        const std::string left = m_editorLineText.substr(0, static_cast<std::size_t>(cursor));
        const std::string right = m_editorLineText.substr(static_cast<std::size_t>(cursor));
        lines[m_editorActiveLine] = left;
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(m_editorActiveLine + 1), right);
        document.text = JoinEditorLines(lines, document.lineEnding);
        m_documents.MarkEdited(elapsedSeconds);
        SetEditorActiveLine(m_editorActiveLine + 1, 0);
    }

    void SlateAppMode::MergeEditorLineWithPrevious(Software::Slate::DocumentService::Document& document,
                                                   std::vector<std::string>& lines,
                                                   double elapsedSeconds)
    {
        if (m_editorActiveLine == 0 || m_editorActiveLine >= lines.size())
        {
            return;
        }

        const std::size_t previous = m_editorActiveLine - 1;
        const int cursor = static_cast<int>(lines[previous].size());
        lines[previous] += m_editorLineText;
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(m_editorActiveLine));
        document.text = JoinEditorLines(lines, document.lineEnding);
        m_documents.MarkEdited(elapsedSeconds);
        SetEditorActiveLine(previous, cursor);
    }

    void SlateAppMode::InsertTextAtEditorCursor(const std::string& text, double elapsedSeconds)
    {
        auto* document = m_documents.Active();
        if (!document || text.empty())
        {
            return;
        }

        auto lines = EditorLinesFromText(document->text);
        if (m_editorActiveLine >= lines.size())
        {
            m_editorActiveLine = lines.size() - 1;
        }

        if (m_editorScratchLine == m_editorActiveLine)
        {
            lines[m_editorActiveLine] = m_editorLineText;
        }
        else
        {
            m_editorLineText = lines[m_editorActiveLine];
            m_editorScratchLine = m_editorActiveLine;
        }

        const int cursor = std::clamp(m_editorCursorPos, 0, static_cast<int>(m_editorLineText.size()));
        const std::string before = m_editorLineText.substr(0, static_cast<std::size_t>(cursor));
        const std::string after = m_editorLineText.substr(static_cast<std::size_t>(cursor));
        const auto replacement = EditorLinesFromText(before + text + after);

        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(m_editorActiveLine));
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(m_editorActiveLine),
                     replacement.begin(),
                     replacement.end());

        document->text = JoinEditorLines(lines, document->lineEnding);
        m_documents.MarkEdited(elapsedSeconds);

        const auto inserted = EditorLinesFromText(text);
        const std::size_t newLine = m_editorActiveLine + (inserted.empty() ? 0 : inserted.size() - 1);
        const int newCursor = inserted.size() <= 1
                                  ? static_cast<int>(before.size() + text.size())
                                  : static_cast<int>(inserted.back().size());
        SetEditorActiveLine(std::min(newLine, lines.size() - 1), newCursor);
    }

    void SlateAppMode::DrawMarkdownLine(const std::string& line, bool inCodeFence, bool inFrontmatter)
    {
        const std::string trimmed = Software::Slate::PathUtils::Trim(line);

        if (trimmed.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, ImGui::GetTextLineHeightWithSpacing()));
            return;
        }

        const float indentWidth = static_cast<float>(LeadingSpaceCount(line)) * 7.0f;
        if (indentWidth > 0.0f)
        {
            ImGui::Indent(indentWidth);
        }

        if (inFrontmatter)
        {
            ImGui::TextColored(Muted, "%s", trimmed.c_str());
        }
        else if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0)
        {
            ImGui::TextColored(Muted, "code");
        }
        else if (inCodeFence)
        {
            ImGui::TextColored(Code, "%s", line.c_str());
        }
        else if (trimmed[0] == '#')
        {
            std::size_t hashes = 0;
            while (hashes < trimmed.size() && trimmed[hashes] == '#')
            {
                ++hashes;
            }
            const std::string title =
                InlineMarkdownText(Software::Slate::PathUtils::Trim(std::string_view(trimmed).substr(hashes)));
            const float scale = hashes == 1 ? 1.42f : (hashes == 2 ? 1.25f : 1.10f);
            DrawScaledText(Cyan, scale, title.empty() ? "heading" : title.c_str());
        }
        else if (trimmed.rfind("- [ ]", 0) == 0 || trimmed.rfind("* [ ]", 0) == 0)
        {
            ImGui::TextColored(Amber, "[ ]");
            ImGui::SameLine();
            const std::string text = InlineMarkdownText(Software::Slate::PathUtils::Trim(trimmed.substr(5)));
            ImGui::TextWrapped("%s", text.c_str());
        }
        else if (trimmed.rfind("- [x]", 0) == 0 || trimmed.rfind("- [X]", 0) == 0 || trimmed.rfind("* [x]", 0) == 0 ||
                 trimmed.rfind("* [X]", 0) == 0)
        {
            ImGui::TextColored(Green, "[x]");
            ImGui::SameLine();
            const std::string text = InlineMarkdownText(Software::Slate::PathUtils::Trim(trimmed.substr(5)));
            ImGui::TextWrapped("%s", text.c_str());
        }
        else if (trimmed[0] == '>')
        {
            ImGui::TextColored(Amber, "|");
            ImGui::SameLine();
            const std::string text = InlineMarkdownText(Software::Slate::PathUtils::Trim(trimmed.substr(1)));
            ImGui::TextColored(Muted, "%s", text.c_str());
        }
        else if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0 || trimmed.rfind("+ ", 0) == 0)
        {
            ImGui::TextColored(Amber, "*");
            ImGui::SameLine();
            const std::string text = InlineMarkdownText(trimmed.substr(2));
            ImGui::TextWrapped("%s", text.c_str());
        }
        else if (!trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0])))
        {
            const std::size_t dot = trimmed.find(". ");
            if (dot != std::string::npos)
            {
                ImGui::TextColored(Amber, "%s", trimmed.substr(0, dot + 1).c_str());
                ImGui::SameLine();
                const std::string text = InlineMarkdownText(trimmed.substr(dot + 2));
                ImGui::TextWrapped("%s", text.c_str());
            }
            else
            {
                const std::string text = InlineMarkdownText(line);
                ImGui::TextWrapped("%s", text.c_str());
            }
        }
        else if (trimmed.find("![](") != std::string::npos || trimmed.find("![") != std::string::npos)
        {
            const std::string text = InlineMarkdownText(trimmed);
            ImGui::TextColored(Amber, "%s", text.c_str());
        }
        else if (trimmed.find("](") != std::string::npos || trimmed.find("[[") != std::string::npos)
        {
            const std::string text = InlineMarkdownText(trimmed);
            ImGui::TextColored(Cyan, "%s", text.c_str());
        }
        else if (trimmed.find('|') != std::string::npos)
        {
            ImGui::TextColored(Code, "%s", trimmed.c_str());
        }
        else
        {
            const std::string text = InlineMarkdownText(line);
            ImGui::TextWrapped("%s", text.c_str());
        }

        if (indentWidth > 0.0f)
        {
            ImGui::Unindent(indentWidth);
        }
    }

    void SlateAppMode::DrawPathList(const char* title, const char* emptyText)
    {
        ImGui::TextColored(Cyan, "%s", title);
        ImGui::Separator();

        if (m_visiblePaths.empty())
        {
            ImGui::TextColored(Muted, "%s", emptyText);
            return;
        }

        for (std::size_t i = 0; i < m_visiblePaths.size(); ++i)
        {
            const bool selected = i == m_navigation.Selected();
            ImGui::TextColored(selected ? Green : Primary, "%s %s", selected ? ">" : " ",
                               m_visiblePaths[i].generic_string().c_str());
        }
    }

    void SlateAppMode::DrawFileTree(bool folderPicker)
    {
        ImGui::TextColored(Cyan, "%s", folderPicker ? "choose folder" : "file tree");
        if (m_filterBuffer[0] != '\0')
        {
            ImGui::SameLine();
            ImGui::TextColored(Amber, " filter: %s", m_filterBuffer.data());
        }
        ImGui::Separator();

        if (m_treeRows.empty())
        {
            ImGui::TextColored(Muted, "no matching entries");
            return;
        }

        for (std::size_t i = 0; i < m_treeRows.size(); ++i)
        {
            const auto& row = m_treeRows[i];
            const bool selected = i == m_navigation.Selected();
            const std::string indent(static_cast<std::size_t>(std::max(0, row.depth)) * 2, ' ');
            const bool collapsed =
                row.isDirectory && m_collapsedFolders.find(Software::Slate::TreePathKey(row.relativePath)) !=
                                       m_collapsedFolders.end();
            const char* marker = selected ? ">" : " ";
            const char* type = row.isDirectory ? (collapsed ? "[+]" : "[-]") : "   ";
            const ImVec4 color = selected ? Green : (row.matchedFilter ? Amber : Primary);
            ImGui::TextColored(color, "%s %s%s %s", marker, indent.c_str(), type, DisplayNameForTreeRow(row).c_str());
        }
    }

    void SlateAppMode::DrawOutline()
    {
        m_visibleHeadings.clear();
        const auto* document = m_documents.Active();
        if (document)
        {
            m_visibleHeadings = m_markdown.Parse(document->text).headings;
        }
        m_navigation.SetCount(m_visibleHeadings.size());

        ImGui::TextColored(Cyan, "outline");
        ImGui::Separator();

        if (m_visibleHeadings.empty())
        {
            ImGui::TextColored(Muted, "no headings");
            return;
        }

        for (std::size_t i = 0; i < m_visibleHeadings.size(); ++i)
        {
            const auto& heading = m_visibleHeadings[i];
            const bool selected = i == m_navigation.Selected();
            std::string indent(static_cast<std::size_t>(std::max(0, heading.level - 1)) * 2, ' ');
            ImGui::TextColored(selected ? Green : Primary, "%s %s%s  line %zu", selected ? ">" : " ", indent.c_str(),
                               heading.title.c_str(), heading.line);
        }
    }

    void SlateAppMode::DrawSearchOverlay()
    {
        m_visibleSearchResults = m_search.Query(m_searchBuffer.data(), 100);
        m_searchNavigation.SetCount(m_visibleSearchResults.size());
        HandleSearchOverlayKeys();
        if (!m_searchOverlayOpen)
        {
            return;
        }

        const ImVec2 size(ImGui::GetWindowWidth() * 0.72f, ImGui::GetWindowHeight() * 0.58f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::BeginChild("SearchOverlay", size, true);
        ImGui::TextColored(Cyan, "search");
        ImGui::SetNextItemWidth(-1.0f);
        if (m_focusFilter)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusFilter = false;
        }
        ImGui::InputText("##SearchQuery", m_searchBuffer.data(), m_searchBuffer.size());
        ImGui::Separator();

        if (m_searchBuffer[0] == '\0')
        {
            ImGui::TextColored(Muted, "type to search across markdown files");
        }
        else if (m_visibleSearchResults.empty())
        {
            ImGui::TextColored(Muted, "no matches");
        }
        else
        {
            for (std::size_t i = 0; i < m_visibleSearchResults.size(); ++i)
            {
                const auto& result = m_visibleSearchResults[i];
                const bool selected = i == m_searchNavigation.Selected();
                ImGui::TextColored(selected ? Green : Primary, "%s %s:%zu", selected ? ">" : " ",
                                   result.relativePath.generic_string().c_str(), result.line);
                ImGui::SameLine();
                ImGui::TextColored(Muted, "%s", result.snippet.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void SlateAppMode::DrawPromptOverlay()
    {
        const ImVec2 size(ImGui::GetWindowWidth() * 0.72f, 92.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f, ImGui::GetWindowHeight() - 150.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::BeginChild("PromptOverlay", size, true);
        ImGui::TextColored(Cyan, "%s", m_promptTitle.c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (m_focusPrompt)
        {
            ImGui::SetKeyboardFocusHere();
            m_focusPrompt = false;
        }
        const bool submitted = ImGui::InputText("##prompt", m_promptBuffer.data(), m_promptBuffer.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted)
        {
            ExecutePrompt();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_promptAction = PromptAction::None;
            m_screen = m_returnScreen;
        }
    }

    void SlateAppMode::DrawConfirm(Software::Core::Runtime::AppContext& context)
    {
        const ImVec2 size(ImGui::GetWindowWidth() * 0.62f, 96.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::BeginChild("ConfirmOverlay", size, true);
        TextCentered(m_confirmMessage.c_str(), m_confirmAction == ConfirmAction::DeletePath ? Red : Amber);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (IsKeyPressed(ImGuiKey_Y) || IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            ExecuteConfirm(true, context);
        }
        else if (IsKeyPressed(ImGuiKey_N))
        {
            ExecuteConfirm(false, context);
        }
        else if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_confirmAction = ConfirmAction::None;
            m_screen = m_returnScreen;
        }
    }

    void SlateAppMode::DrawHelp()
    {
        ImGui::TextColored(Cyan, "shortcuts");
        ImGui::Separator();
        TextLine("arrows", "move selection / document line");
        TextLine("enter", "open, accept, or edit active line");
        TextLine("esc", "back, close, or leave text input");
        TextLine("/", "search overlay, or tree filter in file tree");
        TextLine(":", "command prompt");
        TextLine("^s", "save active document");
        TextLine("n", "new note folder picker");
        TextLine("w", "switch workspaces");
        TextLine("m", "move/rename selected file");
        TextLine("d", "delete selected file with confirmation");
        TextLine("?", "toggle this help");
        TextLine("q", "quit from home");
    }

    void SlateAppMode::DrawStatusLine()
    {
        if (m_filterActive && m_screen == Screen::FileTree && !m_searchOverlayOpen && m_screen != Screen::Prompt &&
            m_screen != Screen::Confirm)
        {
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 82.0f);
            ImGui::TextColored(Amber, "filter");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.42f);
            if (m_focusFilter)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusFilter = false;
            }
            ImGui::InputText("##TreeFilter", m_filterBuffer.data(), m_filterBuffer.size());
        }

        const float helperY = ImGui::GetWindowHeight() - 52.0f;
        ImGui::SetCursorPosY(helperY);
        ImGui::Separator();
        ImGui::TextColored(Muted, "%s", HelperText().c_str());
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 26.0f);
        ImGui::TextColored(m_statusIsError ? Red : Muted, "%s", m_status.c_str());
    }

    std::string SlateAppMode::HelperText() const
    {
        if (m_searchOverlayOpen)
        {
            return "type search   Up/Down select   Enter open   Esc close";
        }
        if (m_screen == Screen::Prompt)
        {
            return "type input   Enter accept   Esc cancel";
        }
        if (m_screen == Screen::Confirm)
        {
            return "Y/Enter accept   N decline   Esc cancel";
        }

        switch (m_screen)
        {
        case Screen::WorkspaceSetup:
            return "(n) new workspace   (:) command   (q) quit";
        case Screen::WorkspaceSwitcher:
            return "Up/Down choose   Enter switch   n new workspace   d remove from list   Esc home";
        case Screen::Home:
            return "(j) journal   (n) new   (o) open   (s) search   (f) files   (w) workspaces   (?) help   (q) quit";
        case Screen::Editor:
            return "Live Preview   active line reveals Markdown   Enter newline   Up/Down line   Ctrl+S save   Ctrl+V image";
        case Screen::FileTree:
            return m_folderPickerActive
                       ? "Up/Down choose   Right expand   Left collapse   Enter select folder   / filter   Esc cancel"
                       : "Up/Down move   Right expand   Left collapse   Enter open/toggle   / filter   n new   m move   d delete   Esc home";
        case Screen::QuickOpen:
        case Screen::Recent:
        case Screen::DocsIndex:
            return "Up/Down move   Enter open   / search   n new   Esc home";
        case Screen::Outline:
            return "Up/Down move   Enter show line   Esc editor";
        case Screen::Help:
            return "Esc close   ? close";
        case Screen::Prompt:
        case Screen::Confirm:
            return {};
        }
        return {};
    }

}
