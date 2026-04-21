#pragma once

#include "App/Slate/AssetService.h"
#include "App/Slate/DocumentService.h"
#include "App/Slate/EditorDocumentViewModel.h"
#include "App/Slate/JournalService.h"
#include "App/Slate/LinkService.h"
#include "App/Slate/MarkdownService.h"
#include "App/Slate/NavigationController.h"
#include "App/Slate/SearchService.h"
#include "App/Slate/ThemeService.h"
#include "App/Slate/WorkspaceService.h"
#include "App/Slate/WorkspaceRegistryService.h"
#include "App/Slate/WorkspaceTree.h"
#include "Core/Runtime/Mode.h"

#include "imgui.h"

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

namespace Software::Modes::Slate
{
    class SlateAppMode final : public Software::Core::Runtime::Mode
    {
    public:
        void OnEnter(Software::Core::Runtime::AppContext& context) override;
        void Update(Software::Core::Runtime::AppContext& context) override;
        void DrawUI(Software::Core::Runtime::AppContext& context) override;

    private:
        enum class Screen
        {
            Home,
            WorkspaceSetup,
            WorkspaceSwitcher,
            Settings,
            Editor,
            QuickOpen,
            FileTree,
            Recent,
            Outline,
            Library,
            Prompt,
            Confirm,
            Help
        };

        enum class PromptAction
        {
            None,
            NewNote,
            NewNoteName,
            Search,
            Workspace,
            WorkspaceTitle,
            WorkspaceEmoji,
            WorkspacePath,
            NewFolderName,
            MoveName,
            RenameMove,
            Command
        };

        enum class ConfirmAction
        {
            None,
            DeletePath,
            ApplyRenameMove,
            RestoreRecovery,
            DiscardRecovery,
            Quit
        };

        enum class SearchOverlayScope
        {
            Workspace,
            Document
        };

        enum class FolderPickerAction
        {
            None,
            NewNote,
            MoveDestination
        };

        void OpenWorkspace(const Software::Slate::fs::path& root);
        void OpenVault(const Software::Slate::WorkspaceVault& vault);
        void BeginWorkspaceCreate();
        void OpenDocument(const Software::Slate::fs::path& relativePath);
        void OpenTodayJournal();
        void CreateNewNote(const std::string& value);
        void CreateNewFolder(const std::string& value);
        void BeginFolderPicker();
        void BeginFolderCreate();
        void BeginMovePicker(const Software::Slate::fs::path& relativePath);
        void BeginSearchOverlay(bool clearQuery, SearchOverlayScope scope = SearchOverlayScope::Workspace);
        void BeginDocumentFindOverlay(bool clearQuery);
        void CloseSearchOverlay();
        void OpenSelectedSearchResult();
        void BeginPrompt(PromptAction action, const std::string& title, const std::string& initialValue = {});
        void ExecutePrompt();
        void ExecuteCommand(const std::string& command);
        void BeginConfirm(ConfirmAction action, const std::string& message);
        void ExecuteConfirm(bool accepted, Software::Core::Runtime::AppContext& context);
        void BeginRenameMove(const Software::Slate::fs::path& relativePath);
        void BeginDelete(const Software::Slate::fs::path& relativePath);
        void PrepareList(Screen screen);
        void PrepareTreeRows(bool folderPicker);
        void OpenSelectedPath();
        void ActivateSelectedTreeRow();
        void ToggleSelectedFolder(bool expanded);
        Software::Slate::fs::path SelectedTreeFolderPath() const;
        bool ApplyPendingRenameMove();
        void CollapseAllWorkspaceFolders();
        bool SaveActiveDocument();
        bool CommitEditorToDocument(double elapsedSeconds);
        void LoadEditorFromActiveDocument();
        void JumpEditorToLine(std::size_t line);
        void ProcessDroppedFiles(Software::Core::Runtime::AppContext& context);

        void DrawRootBegin();
        void DrawScreenContent(Screen screen, Software::Core::Runtime::AppContext& context, bool handleInput);
        void DrawHome(Software::Core::Runtime::AppContext& context);
        void DrawWorkspaceSetup();
        void DrawWorkspaceSwitcher();
        void DrawSettings();
        void DrawEditor(Software::Core::Runtime::AppContext& context);
        void DrawJournalCompanion(const Software::Slate::DocumentService::Document& document);
        void DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary);
        void DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary);
        void DrawLiveMarkdownEditor(Software::Slate::DocumentService::Document& document,
                                    Software::Core::Runtime::AppContext& context);
        void DrawInlineSpans(const std::vector<Software::Slate::MarkdownInlineSpan>& spans, const ImVec4& baseColor);
        void DrawMarkdownLine(const std::string& line, bool inCodeFence, bool inFrontmatter = false);
        void InsertTextAtEditorCursor(const std::string& text, double elapsedSeconds);
        void DrawPathList(const char* title, const char* emptyText);
        void DrawFileTree(bool folderPicker);
        void DrawOutline();
        void DrawSearchOverlay();
        void DrawPromptOverlay();
        void DrawConfirm(Software::Core::Runtime::AppContext& context);
        void DrawHelp();
        void DrawStatusLine();
        std::string HelperText() const;

        void HandleGlobalKeys(Software::Core::Runtime::AppContext& context);
        void HandleListKeys();
        void HandleWorkspaceKeys();
        void HandleSettingsKeys();
        void HandleTreeKeys(bool folderPicker);
        void HandleLibraryKeys();
        void HandleSearchOverlayKeys();
        void HandleHomeKeys(Software::Core::Runtime::AppContext& context);
        void HandleEditorKeys(Software::Core::Runtime::AppContext& context);

        bool IsKeyPressed(ImGuiKey key) const;
        bool IsCtrlPressed(ImGuiKey key) const;
        void SetStatus(std::string message);
        void SetError(std::string message);

        Software::Slate::WorkspaceService m_workspace;
        Software::Slate::DocumentService m_documents;
        Software::Slate::MarkdownService m_markdown;
        Software::Slate::SearchService m_search;
        Software::Slate::JournalService m_journal;
        Software::Slate::LinkService m_links;
        Software::Slate::AssetService m_assets;
        Software::Slate::ThemeService m_theme;
        Software::Slate::WorkspaceRegistryService m_workspaceRegistry;
        Software::Slate::NavigationController m_navigation;
        Software::Slate::NavigationController m_searchNavigation;
        Software::Slate::NavigationController m_workspaceNavigation;
        Software::Slate::EditorDocumentViewModel m_editor;

        Screen m_screen = Screen::Home;
        Screen m_returnScreen = Screen::Home;
        PromptAction m_promptAction = PromptAction::None;
        ConfirmAction m_confirmAction = ConfirmAction::None;

        std::vector<Software::Slate::fs::path> m_visiblePaths;
        std::vector<Software::Slate::TreeViewRow> m_treeRows;
        std::vector<Software::Slate::SearchResult> m_visibleSearchResults;
        std::vector<Software::Slate::Heading> m_visibleHeadings;
        Software::Slate::CollapsedFolderSet m_collapsedFolders;

        Software::Slate::fs::path m_pendingPath;
        Software::Slate::fs::path m_pendingTargetPath;
        Software::Slate::fs::path m_pendingFolderPath;
        Software::Slate::RewritePlan m_pendingRewritePlan;
        std::string m_pendingWorkspaceTitle;
        std::string m_pendingWorkspaceEmoji;

        std::array<char, 1024> m_promptBuffer{};
        std::array<char, 256> m_filterBuffer{};
        std::array<char, 256> m_searchBuffer{};
        std::string m_promptTitle;
        std::string m_confirmMessage;
        std::string m_status = "ready";
        double m_statusSeconds = 0.0;
        bool m_statusIsError = false;
        bool m_searchOverlayOpen = false;
        SearchOverlayScope m_searchOverlayScope = SearchOverlayScope::Workspace;
        Software::Slate::SearchMode m_searchMode = Software::Slate::SearchMode::FileNames;
        bool m_filterActive = false;
        bool m_focusPrompt = false;
        bool m_focusFilter = false;
        bool m_folderPickerActive = false;
        FolderPickerAction m_folderPickerAction = FolderPickerAction::None;
        bool m_workspaceLoaded = false;
        bool m_editorTextFocused = false;
        bool m_journalSummaryValid = false;
        std::size_t m_journalSummaryTextHash = 0;
        double m_journalSummaryUpdatedSeconds = 0.0;
        Software::Slate::fs::path m_journalSummaryPath;
        Software::Slate::JournalMonthSummary m_journalSummary;
        Software::Slate::ThemeSettings m_themeSettings = Software::Slate::ThemeService::DefaultSettings();
        double m_lastScanSeconds = 0.0;
        double m_lastIndexSeconds = 0.0;
        double m_nowSeconds = 0.0;
    };
}
