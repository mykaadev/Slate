#include "Modes/Slate/SlateAppMode.h"

#include "App/Slate/PathUtils.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string_view>
#include <utility>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        std::size_t LeadingSpaceCount(const std::string& line)
        {
            std::size_t count = 0;
            while (count < line.size() && line[count] == ' ')
            {
                ++count;
            }
            return count;
        }

        bool PathEqualsOrDescendantOf(const Software::Slate::fs::path& candidate,
                                      const Software::Slate::fs::path& root)
        {
            const auto normalizedCandidate = Software::Slate::PathUtils::NormalizeRelative(candidate);
            const auto normalizedRoot = Software::Slate::PathUtils::NormalizeRelative(root);
            return normalizedCandidate == normalizedRoot ||
                   Software::Slate::PathIsDescendantOf(normalizedCandidate, normalizedRoot);
        }

        Software::Slate::fs::path RebaseRelativePath(const Software::Slate::fs::path& candidate,
                                                     const Software::Slate::fs::path& oldRoot,
                                                     const Software::Slate::fs::path& newRoot)
        {
            const auto normalizedCandidate = Software::Slate::PathUtils::NormalizeRelative(candidate);
            const auto normalizedOldRoot = Software::Slate::PathUtils::NormalizeRelative(oldRoot);
            Software::Slate::fs::path tail;

            auto candidateIt = normalizedCandidate.begin();
            auto oldIt = normalizedOldRoot.begin();
            for (; oldIt != normalizedOldRoot.end() && candidateIt != normalizedCandidate.end(); ++oldIt, ++candidateIt)
            {
            }
            for (; candidateIt != normalizedCandidate.end(); ++candidateIt)
            {
                tail /= *candidateIt;
            }
            return Software::Slate::PathUtils::NormalizeRelative(
                Software::Slate::PathUtils::NormalizeRelative(newRoot) / tail);
        }

        Software::Slate::fs::path FinalMoveTargetPath(const Software::Slate::fs::path& source,
                                                      const Software::Slate::fs::path& requestedTarget)
        {
            auto target = Software::Slate::PathUtils::NormalizeRelative(requestedTarget);
            if (Software::Slate::PathUtils::IsMarkdownFile(source) && !Software::Slate::PathUtils::IsMarkdownFile(target))
            {
                target += ".md";
            }
            return Software::Slate::PathUtils::NormalizeRelative(target);
        }

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
    }

    void SlateAppMode::OnEnter(Software::Core::Runtime::AppContext& context)
    {
        (void)context;
        m_themeSettings = Software::Slate::ThemeService::DefaultSettings();
        m_theme.Apply(m_themeSettings);
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
        m_nowSeconds = context.frame.elapsedSeconds;
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
            if (!SaveActiveDocument())
            {
                return;
            }
        }

        std::string error;
        if (!m_documents.Open(relativePath, &error))
        {
            SetError(error);
            return;
        }

        if (auto* document = m_documents.Active())
        {
            m_editor.Load(document->text, document->lineEnding);
            m_editorTextFocused = true;
        }
        m_journalSummaryValid = false;

        m_workspace.TouchRecent(relativePath);
        m_screen = Screen::Editor;
        m_returnScreen = Screen::Home;
        m_folderPickerActive = false;
        m_folderPickerAction = FolderPickerAction::None;
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
        m_folderPickerAction = FolderPickerAction::None;
        OpenDocument(created);
    }

    void SlateAppMode::CreateNewFolder(const std::string& value)
    {
        Software::Slate::fs::path created;
        std::string error;
        if (!m_workspace.CreateFolder(m_pendingFolderPath, value, &created, &error))
        {
            SetError(error);
            m_screen = Screen::FileTree;
            return;
        }

        m_collapsedFolders.erase(Software::Slate::TreePathKey(m_pendingFolderPath));
        m_collapsedFolders.insert(Software::Slate::TreePathKey(created));
        m_search.Rebuild(m_workspace);
        m_screen = Screen::FileTree;
        SetStatus("created folder " + created.generic_string());
    }

    void SlateAppMode::BeginFolderPicker()
    {
        m_pendingFolderPath = ".";
        m_folderPickerActive = true;
        m_folderPickerAction = FolderPickerAction::NewNote;
        m_screen = Screen::FileTree;
        std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
        m_filterActive = false;
        m_navigation.Reset();
        SetStatus("choose a folder for the new note");
    }

    void SlateAppMode::BeginFolderCreate()
    {
        m_pendingFolderPath = SelectedTreeFolderPath();
        BeginPrompt(PromptAction::NewFolderName, "new folder name", "New Folder");
    }

    void SlateAppMode::BeginMovePicker(const Software::Slate::fs::path& relativePath)
    {
        m_pendingPath = Software::Slate::PathUtils::NormalizeRelative(relativePath);
        const auto parent = m_pendingPath.parent_path();
        m_pendingFolderPath = parent.empty() ? Software::Slate::fs::path(".") : parent;
        m_pendingRewritePlan = {};
        m_folderPickerActive = true;
        m_folderPickerAction = FolderPickerAction::MoveDestination;
        m_screen = Screen::FileTree;
        std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
        m_filterActive = false;
        m_navigation.Reset();
        SetStatus("choose destination for " + m_pendingPath.generic_string());
    }

    void SlateAppMode::BeginSearchOverlay(bool clearQuery, SearchOverlayScope scope)
    {
        CommitEditorToDocument(m_nowSeconds);
        if (clearQuery)
        {
            std::fill(m_searchBuffer.begin(), m_searchBuffer.end(), '\0');
            m_searchNavigation.Reset();
        }
        m_searchOverlayScope = scope;
        if (scope == SearchOverlayScope::Workspace)
        {
            m_searchMode = Software::Slate::SearchMode::FileNames;
        }
        m_searchOverlayOpen = true;
        m_focusFilter = true;
    }

    void SlateAppMode::BeginDocumentFindOverlay(bool clearQuery)
    {
        BeginSearchOverlay(clearQuery, SearchOverlayScope::Document);
    }

    void SlateAppMode::CloseSearchOverlay()
    {
        m_searchOverlayOpen = false;
    }

    void SlateAppMode::OpenSelectedSearchResult()
    {
        if (!m_searchNavigation.HasSelection() || m_visibleSearchResults.empty())
        {
            return;
        }

        const auto result = m_visibleSearchResults[m_searchNavigation.Selected()];
        CloseSearchOverlay();
        if (m_searchOverlayScope == SearchOverlayScope::Document)
        {
            JumpEditorToLine(result.line);
            m_screen = Screen::Editor;
            return;
        }

        OpenDocument(result.relativePath);
        if (m_searchMode == Software::Slate::SearchMode::FullText && result.line > 0)
        {
            JumpEditorToLine(result.line);
        }
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
        case PromptAction::NewFolderName:
            CreateNewFolder(value);
            break;
        case PromptAction::MoveName:
        {
            const auto source = Software::Slate::PathUtils::NormalizeRelative(m_pendingPath);
            const std::string finalName = value.empty() ? source.filename().string() : value;
            if (finalName.empty())
            {
                SetError("target name required");
                m_screen = Screen::FileTree;
                break;
            }

            Software::Slate::fs::path folder = Software::Slate::PathUtils::NormalizeRelative(m_pendingFolderPath);
            if (folder == ".")
            {
                folder.clear();
            }

            m_pendingTargetPath = FinalMoveTargetPath(source, folder / finalName);
            m_pendingRewritePlan = m_links.BuildRenamePlan(m_workspace, source, m_pendingTargetPath);
            m_folderPickerActive = false;
            m_folderPickerAction = FolderPickerAction::None;
            if (m_pendingRewritePlan.TotalReplacements() > 0)
            {
                BeginConfirm(ConfirmAction::ApplyRenameMove,
                             "move updates " + std::to_string(m_pendingRewritePlan.TotalReplacements()) +
                                 " links. y apply / esc cancel");
            }
            else
            {
                ApplyPendingRenameMove();
                m_screen = Screen::FileTree;
            }
            break;
        }
        case PromptAction::RenameMove:
        {
            if (value.empty())
            {
                SetError("target path required");
                m_screen = Screen::FileTree;
                break;
            }
            m_pendingTargetPath = FinalMoveTargetPath(m_pendingPath, value);
            m_pendingRewritePlan = m_links.BuildRenamePlan(m_workspace, m_pendingPath, m_pendingTargetPath);
            if (m_pendingRewritePlan.TotalReplacements() > 0)
            {
                BeginConfirm(ConfirmAction::ApplyRenameMove,
                             "rename/move updates " + std::to_string(m_pendingRewritePlan.TotalReplacements()) +
                                 " links. y apply / esc cancel");
            }
            else
            {
                ApplyPendingRenameMove();
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
        else if (trimmed == "folder" || trimmed == "mkdir")
        {
            BeginFolderCreate();
        }
        else if (trimmed == "workspaces" || trimmed == "vaults")
        {
            m_workspaceNavigation.SetCount(m_workspaceRegistry.Vaults().size());
            m_screen = Screen::WorkspaceSwitcher;
        }
        else if (trimmed == "t" || trimmed == "theme" || trimmed == "settings")
        {
            m_navigation.SetCount(2);
            m_navigation.Reset();
            m_screen = Screen::Settings;
        }
        else if (trimmed == "library" || trimmed == "l" || trimmed == "docs")
        {
            std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
            m_folderPickerActive = false;
            m_folderPickerAction = FolderPickerAction::None;
            m_navigation.Reset();
            m_screen = Screen::Library;
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
        {
            const auto* active = m_documents.Active();
            const bool activeWasDeleted =
                active && PathEqualsOrDescendantOf(active->relativePath, m_pendingPath);
            if (m_workspace.DeletePath(m_pendingPath, &error))
            {
                SetStatus("deleted " + m_pendingPath.generic_string());
                m_search.Rebuild(m_workspace);
                if (activeWasDeleted)
                {
                    m_documents.Close();
                    m_editorTextFocused = false;
                    m_editor.Load("", "\n");
                }
            }
            else
            {
                SetError(error);
            }
            m_screen = Screen::FileTree;
            break;
        }
        case ConfirmAction::ApplyRenameMove:
            if (!m_links.ApplyRewritePlan(m_workspace, m_pendingRewritePlan, &error))
            {
                SetError(error);
                m_screen = Screen::FileTree;
                break;
            }
            ApplyPendingRenameMove();
            m_screen = Screen::FileTree;
            break;
        case ConfirmAction::RestoreRecovery:
            if (m_documents.RestoreRecovery(&error))
            {
                if (auto* document = m_documents.Active())
                {
                    m_editor.Load(document->text, document->lineEnding);
                    m_editorTextFocused = true;
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
            if (SaveActiveDocument())
            {
                context.quitRequested = true;
            }
            else
            {
                m_screen = m_returnScreen;
            }
            break;
        case ConfirmAction::None:
            m_screen = m_returnScreen;
            break;
        }
    }

    void SlateAppMode::BeginRenameMove(const Software::Slate::fs::path& relativePath)
    {
        BeginMovePicker(relativePath);
    }

    void SlateAppMode::BeginDelete(const Software::Slate::fs::path& relativePath)
    {
        m_pendingPath = relativePath;
        BeginConfirm(ConfirmAction::DeletePath,
                     "delete file/folder " + relativePath.generic_string() + "? y delete / esc cancel");
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
            if (m_folderPickerAction == FolderPickerAction::MoveDestination &&
                Software::Slate::fs::is_directory(m_workspace.Resolve(m_pendingPath)))
            {
                rows.erase(std::remove_if(rows.begin(), rows.end(),
                                          [&](const Software::Slate::TreeViewRow& row) {
                                              return PathEqualsOrDescendantOf(row.relativePath, m_pendingPath);
                                          }),
                           rows.end());
            }
            for (auto& row : rows)
            {
                row.depth += 1;
                row.selectable = true;
                m_treeRows.push_back(std::move(row));
            }
        }
        else
        {
            const auto mode = m_screen == Screen::Library ? Software::Slate::TreeViewMode::Library
                                                          : Software::Slate::TreeViewMode::Files;
            m_treeRows =
                Software::Slate::BuildTreeRows(m_workspace.Entries(), m_collapsedFolders, m_filterBuffer.data(), mode);
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
                if (m_folderPickerAction == FolderPickerAction::MoveDestination)
                {
                    BeginPrompt(PromptAction::MoveName, "move as", m_pendingPath.filename().string());
                }
                else
                {
                    BeginPrompt(PromptAction::NewNoteName, "new note name", "Untitled.md");
                }
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

    Software::Slate::fs::path SlateAppMode::SelectedTreeFolderPath() const
    {
        if (!m_navigation.HasSelection() || m_treeRows.empty())
        {
            return ".";
        }

        const auto& row = m_treeRows[m_navigation.Selected()];
        if (row.isDirectory)
        {
            return row.relativePath;
        }

        const auto parent = row.relativePath.parent_path();
        return parent.empty() ? Software::Slate::fs::path(".") : parent;
    }

    bool SlateAppMode::ApplyPendingRenameMove()
    {
        const auto source = Software::Slate::PathUtils::NormalizeRelative(m_pendingPath);
        const auto target = FinalMoveTargetPath(source, m_pendingTargetPath);
        m_pendingTargetPath = target;
        const bool sourceIsDirectory = Software::Slate::fs::is_directory(m_workspace.Resolve(source));

        if (source == target)
        {
            SetError("source and target are the same");
            return false;
        }

        if (sourceIsDirectory && PathEqualsOrDescendantOf(target, source))
        {
            SetError("cannot move a folder into itself");
            return false;
        }

        bool activeWasMoved = false;
        Software::Slate::fs::path movedActivePath;
        if (const auto* active = m_documents.Active())
        {
            activeWasMoved = PathEqualsOrDescendantOf(active->relativePath, source);
            if (activeWasMoved)
            {
                movedActivePath = RebaseRelativePath(active->relativePath, source, target);
                if (!SaveActiveDocument())
                {
                    return false;
                }
            }
        }

        std::string error;
        if (!m_workspace.RenameOrMove(source, target, &error))
        {
            SetError(error);
            return false;
        }

        SetStatus("moved " + source.generic_string() + " -> " + target.generic_string());
        m_search.Rebuild(m_workspace);
        const auto targetParent = target.parent_path();
        m_collapsedFolders.erase(Software::Slate::TreePathKey(targetParent.empty() ? Software::Slate::fs::path(".") : targetParent));
        if (sourceIsDirectory)
        {
            m_collapsedFolders.insert(Software::Slate::TreePathKey(target));
        }

        if (activeWasMoved)
        {
            m_documents.Close();
            m_editorTextFocused = false;
            m_editor.Load("", "\n");
            if (Software::Slate::PathUtils::IsMarkdownFile(movedActivePath) &&
                Software::Slate::fs::exists(m_workspace.Resolve(movedActivePath)))
            {
                OpenDocument(movedActivePath);
            }
        }

        return true;
    }

    void SlateAppMode::CollapseAllWorkspaceFolders()
    {
        m_collapsedFolders.clear();
        for (const auto& entry : m_workspace.Entries())
        {
            if (entry.isDirectory)
            {
                m_collapsedFolders.insert(Software::Slate::TreePathKey(entry.relativePath));
            }
        }
    }

    bool SlateAppMode::SaveActiveDocument()
    {
        CommitEditorToDocument(m_nowSeconds);
        std::string error;
        if (m_documents.Save(&error))
        {
            SetStatus("saved");
            m_workspace.Scan(nullptr);
            m_search.Rebuild(m_workspace);
            return true;
        }
        SetError(error);
        return false;
    }

    bool SlateAppMode::CommitEditorToDocument(double elapsedSeconds)
    {
        auto* document = m_documents.Active();
        if (!document)
        {
            return false;
        }

        std::string text;
        const bool changed = m_editor.CommitActiveLine(&text);
        if (changed || document->text != text)
        {
            document->text = std::move(text);
            m_documents.MarkEdited(elapsedSeconds);
            return true;
        }
        return false;
    }

    void SlateAppMode::LoadEditorFromActiveDocument()
    {
        if (const auto* document = m_documents.Active())
        {
            m_editor.Load(document->text, document->lineEnding);
            m_editorTextFocused = true;
        }
        m_journalSummaryValid = false;
    }

    void SlateAppMode::JumpEditorToLine(std::size_t line)
    {
        if (line == 0)
        {
            return;
        }
        m_editor.SetActiveLine(line - 1, 0);
        m_editorTextFocused = true;
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
        case Screen::Settings:
            if (handleInput)
            {
                HandleSettingsKeys();
            }
            DrawSettings();
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
            PrepareList(screen);
            if (handleInput)
            {
                HandleListKeys();
            }
            DrawPathList(screen == Screen::QuickOpen ? "quick open" : "recent files", "no matching files");
            break;
        case Screen::Library:
            PrepareTreeRows(false);
            if (handleInput)
            {
                HandleLibraryKeys();
            }
            DrawFileTree(false);
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
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.10f));
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
        TextCentered("quiet markdown workspace", Primary);

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const float columnWidth = 360.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - columnWidth) * 0.5f);
        ImGui::BeginGroup();
        auto section = [](const char* title) {
            ImGui::Dummy(ImVec2(1.0f, 8.0f));
            ImGui::TextColored(Muted, "%s", title);
        };
        section("write");
        TextLine("j", "today journal");
        TextLine("n", "new note");
        section("browse");
        TextLine("l", "library");
        TextLine("f", "file tree");
        TextLine("r", "recent files");
        section("find");
        TextLine("s", "search");
        TextLine("o", "quick open");
        section("system");
        TextLine("w", "workspaces");
        TextLine("t", "theme");
        TextLine("?", "shortcuts");
        TextLine("q", "quit");
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        const std::string stat = std::to_string(m_workspace.MarkdownFiles().size()) + " markdown files";
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

        ImGui::TextColored(Muted, "%s", document->relativePath.generic_string().c_str());
        ImGui::SameLine();
        ImGui::TextColored(document->conflict ? Red : (document->dirty ? Amber : Green), "%s",
                           document->conflict ? "external change" : (document->dirty ? "dirty" : "saved"));

        if (m_journal.IsJournalPath(document->relativePath))
        {
            ImGui::Dummy(ImVec2(1.0f, 10.0f));
            DrawJournalCompanion(*document);
            ImGui::Dummy(ImVec2(1.0f, 12.0f));
        }

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("LiveMarkdownEditorPage", ImVec2(0.0f, avail.y - 52.0f), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        DrawLiveMarkdownEditor(*document, context);
        ImGui::EndChild();
    }

    void SlateAppMode::DrawSettings()
    {
        m_navigation.SetCount(2);

        ImGui::TextColored(Cyan, "settings");
        ImGui::Separator();
        ImGui::TextColored(Muted, "Theme changes save to this workspace.");
        ImGui::Dummy(ImVec2(1.0f, 10.0f));

        const bool shellSelected = m_navigation.Selected() == 0;
        const bool markdownSelected = m_navigation.Selected() == 1;
        ImGui::TextColored(shellSelected ? Green : Primary, "%s overall theme     %s", shellSelected ? ">" : " ",
                           Software::Slate::ThemeService::ShellPresetLabel(m_themeSettings.shellPreset).c_str());
        ImGui::TextColored(markdownSelected ? Green : Primary, "%s markdown colors  %s",
                           markdownSelected ? ">" : " ",
                           Software::Slate::ThemeService::MarkdownPresetLabel(m_themeSettings.markdownPreset).c_str());

        ImGui::Dummy(ImVec2(1.0f, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(1.0f, 8.0f));
        ImGui::TextColored(Muted, "preview");
        ImGui::Dummy(ImVec2(1.0f, 6.0f));

        bool previewInCodeFence = false;
        DrawMarkdownLine("# Encounter Notes", previewInCodeFence);
        DrawMarkdownLine("## Combat Loop", previewInCodeFence);
        DrawMarkdownLine("- [ ] tune parry window", previewInCodeFence);
        DrawMarkdownLine("- [x] player dodge feels right", previewInCodeFence);
        DrawMarkdownLine("> pace matters more than particles", previewInCodeFence);
        DrawMarkdownLine("A [link](Docs/Combat.md), **bold**, *italic*, and `code`.", previewInCodeFence);
        DrawMarkdownLine("| stat | value |", previewInCodeFence);
        DrawMarkdownLine("![](assets/encounter/board.png)", previewInCodeFence);
    }

    void SlateAppMode::DrawJournalCompanion(const Software::Slate::DocumentService::Document& document)
    {
        const std::size_t textHash = std::hash<std::string>{}(document.text);
        const bool pathChanged = !m_journalSummaryValid || m_journalSummaryPath != document.relativePath;
        const bool textChanged = !m_journalSummaryValid || m_journalSummaryTextHash != textHash;
        const bool refreshDue = m_nowSeconds - m_journalSummaryUpdatedSeconds >= (textChanged ? 0.35 : 2.0);
        if (pathChanged || (textChanged && refreshDue) || (!textChanged && refreshDue))
        {
            m_journalSummary = m_journal.BuildCurrentMonthSummary(m_workspace, document.relativePath, &document.text);
            m_journalSummaryPath = document.relativePath;
            m_journalSummaryTextHash = textHash;
            m_journalSummaryUpdatedSeconds = m_nowSeconds;
            m_journalSummaryValid = true;
        }

        const auto& summary = m_journalSummary;
        char title[64];
        std::snprintf(title, sizeof(title), "%s %d", MonthName(summary.month), summary.year);
        ImGui::TextColored(Cyan, "%s", title);
        ImGui::SameLine();

        std::string meta = std::to_string(summary.writtenDays) + " written";
        meta += "   streak " + std::to_string(summary.currentStreak);
        if (summary.activeWordCount > 0)
        {
            meta += "   " + std::to_string(summary.activeWordCount) + " words";
        }
        ImGui::TextColored(Muted, "%s", meta.c_str());

        ImGui::TextColored(Amber, "prompt");
        ImGui::SameLine();
        ImGui::TextColored(Primary, "%s", summary.prompt.c_str());
        ImGui::Dummy(ImVec2(1.0f, 6.0f));
        DrawJournalCalendar(summary);
        ImGui::Dummy(ImVec2(1.0f, 8.0f));
        DrawJournalActivityGraph(summary);
    }

    void SlateAppMode::DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary)
    {
        static constexpr std::array<const char*, 7> Weekdays{{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"}};
        if (!ImGui::BeginTable("JournalMonthCalendar", 7,
                               ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            return;
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
                ImVec4 dayColor = Primary;
                if (day == summary.currentDay)
                {
                    dayColor = Cyan;
                }
                else if (day == summary.activeDay)
                {
                    dayColor = Amber;
                }
                else if (daySummary.hasContent)
                {
                    dayColor = Green;
                }

                char dayBuffer[8];
                std::snprintf(dayBuffer, sizeof(dayBuffer), "%2d", day);
                ImGui::TextColored(dayColor, "%s", dayBuffer);
                if (daySummary.hasContent)
                {
                    ImGui::SameLine(0.0f, 4.0f);
                    ImGui::TextColored(Green, "x");
                }
                ++day;
            }
        }

        ImGui::EndTable();
    }

    void SlateAppMode::DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary)
    {
        const float width = std::min(ImGui::GetContentRegionAvail().x, 360.0f);
        const float barHeight = 42.0f;
        const float labelHeight = 16.0f;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, barHeight + labelHeight);
        ImGui::InvisibleButton("JournalActivityGraph", size);

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(start.x, start.y + barHeight), ImVec2(start.x + width, start.y + barHeight),
                          ImGui::ColorConvertFloat4ToU32(Muted));

        int maxWords = 0;
        for (const int value : summary.recentWordCounts)
        {
            maxWords = std::max(maxWords, value);
        }

        const float slotWidth = width / 7.0f;
        for (int i = 0; i < 7; ++i)
        {
            const int dayNumber = summary.recentDayNumbers[static_cast<std::size_t>(i)];
            const int words = summary.recentWordCounts[static_cast<std::size_t>(i)];
            const float x0 = start.x + slotWidth * static_cast<float>(i) + 6.0f;
            const float x1 = start.x + slotWidth * static_cast<float>(i + 1) - 6.0f;
            const float normalizedHeight = maxWords > 0 ? (static_cast<float>(words) / static_cast<float>(maxWords)) : 0.0f;
            const float height = words > 0 ? std::max(6.0f, normalizedHeight * (barHeight - 6.0f)) : 2.0f;
            ImVec4 color = words > 0 ? Green : Muted;
            if (dayNumber == summary.currentDay && dayNumber > 0)
            {
                color = Cyan;
            }

            drawList->AddRectFilled(ImVec2(x0, start.y + barHeight - height), ImVec2(x1, start.y + barHeight),
                                    ImGui::ColorConvertFloat4ToU32(color), 2.0f);

            if (dayNumber > 0)
            {
                char label[8];
                std::snprintf(label, sizeof(label), "%d", dayNumber);
                drawList->AddText(ImVec2(x0, start.y + barHeight + 2.0f), ImGui::ColorConvertFloat4ToU32(Muted), label);
            }
        }
    }

    void SlateAppMode::DrawLiveMarkdownEditor(Software::Slate::DocumentService::Document& document,
                                              Software::Core::Runtime::AppContext& context)
    {
        m_editor.EnsureLoaded(document.text, document.lineEnding);
        const auto& lines = m_editor.Lines();

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
            if (i == m_editor.ActiveLine())
            {
                if (m_editor.FocusRequested())
                {
                    ImGui::SetKeyboardFocusHere();
                    m_editor.ClearFocusRequest();
                }

                ImGui::SetNextItemWidth(pageWidth);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.095f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, Primary);
                int cursor = m_editor.CaretColumn();
                const int cursorBeforeInput = cursor;
                const int lineSizeBeforeInput = static_cast<int>(m_editor.ActiveLineText().size());
                const TextInputResult result = InputTextString("##LiveMarkdownLine", m_editor.ActiveLineText(),
                                                               ImGuiInputTextFlags_EnterReturnsTrue,
                                                               &cursor,
                                                               m_editor.RequestedCursorColumn());
                m_editor.SetCaretColumn(cursor);
                m_editor.ClearRequestedCursorColumn();
                m_editorTextFocused = ImGui::IsItemActive();
                ImGui::PopStyleColor(2);

                if (result.edited)
                {
                    CommitEditorToDocument(context.frame.elapsedSeconds);
                }

                if (result.submitted)
                {
                    m_editor.SplitActiveLine();
                    CommitEditorToDocument(context.frame.elapsedSeconds);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && ImGui::IsKeyPressed(ImGuiKey_Backspace, true) &&
                    cursorBeforeInput <= 0 && m_editor.ActiveLine() > 0)
                {
                    if (m_editor.MergeActiveLineWithPrevious())
                    {
                        CommitEditorToDocument(context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && ImGui::IsKeyPressed(ImGuiKey_Delete, true) &&
                    cursorBeforeInput >= lineSizeBeforeInput)
                {
                    if (m_editor.MergeActiveLineWithNext())
                    {
                        CommitEditorToDocument(context.frame.elapsedSeconds);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) && m_editor.ActiveLine() > 0)
                {
                    m_editor.MoveActiveLine(-1);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }

                if (m_editorTextFocused && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) &&
                    m_editor.ActiveLine() + 1 < lines.size())
                {
                    m_editor.MoveActiveLine(1);
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
                m_editor.SetActiveLine(i, static_cast<int>(lines[i].size()));
                m_editorTextFocused = true;
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

    void SlateAppMode::InsertTextAtEditorCursor(const std::string& text, double elapsedSeconds)
    {
        auto* document = m_documents.Active();
        if (!document || text.empty())
        {
            return;
        }

        m_editor.EnsureLoaded(document->text, document->lineEnding);
        if (m_editor.InsertTextAtCursor(text))
        {
            CommitEditorToDocument(elapsedSeconds);
        }
    }

    void SlateAppMode::DrawInlineSpans(const std::vector<Software::Slate::MarkdownInlineSpan>& spans,
                                       const ImVec4& baseColor)
    {
        bool first = true;
        for (const auto& span : spans)
        {
            if (span.text.empty())
            {
                continue;
            }
            if (!first)
            {
                ImGui::SameLine(0.0f, 0.0f);
            }

            ImVec4 color = baseColor;
            if (span.code)
            {
                color = MarkdownCode;
            }
            else if (span.image)
            {
                color = MarkdownImage;
            }
            else if (span.link)
            {
                color = MarkdownLink;
            }
            else if (span.bold)
            {
                color = MarkdownBold;
            }
            else if (span.italic)
            {
                color = MarkdownItalic;
            }

            ImGui::TextColored(color, "%s", span.text.c_str());
            if (span.bold)
            {
                const ImVec2 min = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 0.7f, min.y), ImGui::ColorConvertFloat4ToU32(color),
                                                    span.text.c_str());
            }
            first = false;
        }

        if (first)
        {
            ImGui::TextUnformatted("");
        }
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
            ImGui::TextColored(MarkdownCode, "code");
        }
        else if (inCodeFence)
        {
            ImGui::TextColored(MarkdownCode, "%s", line.c_str());
        }
        else if (trimmed[0] == '#')
        {
            std::size_t hashes = 0;
            while (hashes < trimmed.size() && trimmed[hashes] == '#')
            {
                ++hashes;
            }
            const std::string title =
                Software::Slate::PathUtils::Trim(std::string_view(trimmed).substr(hashes));
            const float scale = hashes == 1 ? 1.42f : (hashes == 2 ? 1.25f : 1.10f);
            const ImVec4 headingColor = hashes == 1 ? MarkdownHeading1 : (hashes == 2 ? MarkdownHeading2 : MarkdownHeading3);
            ImGui::SetWindowFontScale(scale);
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(title.empty() ? "heading" : title),
                            headingColor);
            ImGui::SetWindowFontScale(1.0f);
        }
        else if (trimmed.rfind("- [ ]", 0) == 0 || trimmed.rfind("* [ ]", 0) == 0)
        {
            ImGui::TextColored(MarkdownCheckbox, "[ ]");
            ImGui::SameLine();
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(
                                Software::Slate::PathUtils::Trim(trimmed.substr(5))),
                            Primary);
        }
        else if (trimmed.rfind("- [x]", 0) == 0 || trimmed.rfind("- [X]", 0) == 0 || trimmed.rfind("* [x]", 0) == 0 ||
                 trimmed.rfind("* [X]", 0) == 0)
        {
            ImGui::TextColored(MarkdownCheckboxDone, "[x]");
            ImGui::SameLine();
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(
                                Software::Slate::PathUtils::Trim(trimmed.substr(5))),
                            Primary);
        }
        else if (trimmed[0] == '>')
        {
            ImGui::TextColored(MarkdownQuote, "|");
            ImGui::SameLine();
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(
                                Software::Slate::PathUtils::Trim(trimmed.substr(1))),
                            MarkdownQuote);
        }
        else if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0 || trimmed.rfind("+ ", 0) == 0)
        {
            ImGui::TextColored(MarkdownBullet, "*");
            ImGui::SameLine();
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(trimmed.substr(2)), Primary);
        }
        else if (!trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0])))
        {
            const std::size_t dot = trimmed.find(". ");
            if (dot != std::string::npos)
            {
                ImGui::TextColored(MarkdownBullet, "%s", trimmed.substr(0, dot + 1).c_str());
                ImGui::SameLine();
                DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(trimmed.substr(dot + 2)), Primary);
            }
            else
            {
                DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(line), Primary);
            }
        }
        else if (trimmed.find("![](") != std::string::npos || trimmed.find("![") != std::string::npos)
        {
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(trimmed), MarkdownImage);
        }
        else if (trimmed.find("](") != std::string::npos || trimmed.find("[[") != std::string::npos)
        {
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(trimmed), Primary);
        }
        else if (trimmed.find('|') != std::string::npos)
        {
            ImGui::TextColored(MarkdownTable, "%s", trimmed.c_str());
        }
        else
        {
            DrawInlineSpans(Software::Slate::MarkdownService::ParseInlineSpans(line), Primary);
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
        const bool movePicker = folderPicker && m_folderPickerAction == FolderPickerAction::MoveDestination;
        const char* title =
            m_screen == Screen::Library ? "library" : (movePicker ? "move to folder" : (folderPicker ? "choose folder" : "file tree"));
        ImGui::TextColored(Cyan, "%s", title);
        if (m_filterBuffer[0] != '\0')
        {
            ImGui::SameLine();
            ImGui::TextColored(Amber, " filter: %s", m_filterBuffer.data());
        }
        if (movePicker)
        {
            ImGui::TextColored(Muted, "moving: %s", m_pendingPath.generic_string().c_str());
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
        if (m_searchOverlayScope == SearchOverlayScope::Document)
        {
            const auto* document = m_documents.Active();
            m_visibleSearchResults = document ? Software::Slate::SearchService::QueryDocument(document->text,
                                                                                              m_searchBuffer.data(),
                                                                                              100)
                                              : std::vector<Software::Slate::SearchResult>{};
        }
        else
        {
            m_visibleSearchResults = m_search.Query(m_searchBuffer.data(), m_searchMode, 100);
        }
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
        const bool documentFind = m_searchOverlayScope == SearchOverlayScope::Document;
        ImGui::TextColored(Cyan, "%s", documentFind ? "find in file" : "search");
        if (!documentFind)
        {
            ImGui::SameLine();
            ImGui::TextColored(Muted, "%s", m_searchMode == Software::Slate::SearchMode::FileNames
                                             ? "files"
                                             : "full text");
        }
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
            ImGui::TextColored(Muted, "%s", documentFind ? "type to find in this file"
                                                         : "type to search filenames. Tab toggles full text");
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
                if (documentFind)
                {
                    ImGui::TextColored(selected ? Green : Primary, "%s line %zu:%zu", selected ? ">" : " ",
                                       result.line, result.column);
                }
                else
                {
                    std::string label = result.relativePath.generic_string();
                    if (m_searchMode == Software::Slate::SearchMode::FullText)
                    {
                        label += ":" + std::to_string(result.line);
                    }
                    ImGui::TextColored(selected ? Green : Primary, "%s %s", selected ? ">" : " ", label.c_str());
                }
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
        TextLine("l", "library");
        TextLine("t", "theme settings");
        TextLine("a", "create folder in file tree");
        TextLine("w", "switch workspaces");
        TextLine("m", "move/rename selected file or folder with folder picker");
        TextLine("d / del", "delete selected file or folder with confirmation");
        TextLine("?", "toggle this help");
        TextLine("q", "quit from home");
    }

    void SlateAppMode::DrawStatusLine()
    {
        if (m_filterActive && (m_screen == Screen::FileTree || m_screen == Screen::Library) && !m_searchOverlayOpen &&
            m_screen != Screen::Prompt && m_screen != Screen::Confirm)
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
        ImVec4 statusColor = m_statusIsError ? Red : Muted;
        if (!m_statusIsError)
        {
            const float age = static_cast<float>(std::max(0.0, m_nowSeconds - m_statusSeconds));
            statusColor.w = std::clamp(1.0f - age / 4.0f, 0.22f, 1.0f);
        }
        ImGui::TextColored(statusColor, "%s", m_status.c_str());
    }

    std::string SlateAppMode::HelperText() const
    {
        if (m_searchOverlayOpen)
        {
            if (m_searchOverlayScope == SearchOverlayScope::Document)
            {
                return "find in file   Up/Down select   Enter jump   Shift+Enter previous   Esc close";
            }
            return m_searchMode == Software::Slate::SearchMode::FileNames
                       ? "filename search   Tab full text   Up/Down select   Enter open   Esc close"
                       : "full text search   Tab filenames   Up/Down select   Enter open   Esc close";
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
            return "(j) journal   (n) new   (l) library   (t) theme   (?) help";
        case Screen::Settings:
            return "Up/Down choose   Left/Right change   Enter next   R reset   Esc home";
        case Screen::Editor:
            return "Live Preview   Ctrl+F find   Ctrl+S save   Esc leave editor";
        case Screen::Library:
            return "Up/Down move   Right expand   Left collapse   Enter open/toggle   / filter   Esc home";
        case Screen::FileTree:
            if (m_folderPickerActive && m_folderPickerAction == FolderPickerAction::MoveDestination)
            {
                return "Up/Down choose destination   Right expand   Left collapse   Enter choose   a folder   / filter   Esc cancel";
            }
            return m_folderPickerActive
                       ? "Up/Down choose   Right expand   Left collapse   Enter select folder   a folder   / filter   Esc cancel"
                       : "Up/Down move   Right expand   Left collapse   Enter open/toggle   n note   a folder   m move   d/Delete delete   / filter   Esc home";
        case Screen::QuickOpen:
        case Screen::Recent:
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
