#pragma once

#include "App/Slate/AssetService.h"
#include "App/Slate/DocumentService.h"
#include "App/Slate/LinkService.h"
#include "App/Slate/MarkdownService.h"
#include "App/Slate/NavigationController.h"
#include "App/Slate/SearchService.h"
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
            Editor,
            QuickOpen,
            FileTree,
            Recent,
            Outline,
            DocsIndex,
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

        void OpenWorkspace(const Software::Slate::fs::path& root);
        void OpenVault(const Software::Slate::WorkspaceVault& vault);
        void BeginWorkspaceCreate();
        void OpenDocument(const Software::Slate::fs::path& relativePath);
        void OpenTodayJournal();
        void CreateNewNote(const std::string& value);
        void BeginFolderPicker();
        void BeginSearchOverlay(bool clearQuery);
        void CloseSearchOverlay();
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
        void SaveActiveDocument();
        void ProcessDroppedFiles(Software::Core::Runtime::AppContext& context);

        void DrawRootBegin();
        void DrawScreenContent(Screen screen, Software::Core::Runtime::AppContext& context, bool handleInput);
        void DrawHome(Software::Core::Runtime::AppContext& context);
        void DrawWorkspaceSetup();
        void DrawWorkspaceSwitcher();
        void DrawEditor(Software::Core::Runtime::AppContext& context);
        void DrawLiveMarkdownEditor(Software::Slate::DocumentService::Document& document,
                                    Software::Core::Runtime::AppContext& context);
        void DrawMarkdownLine(const std::string& line, bool inCodeFence, bool inFrontmatter = false);
        void SetEditorActiveLine(std::size_t line, int requestedCursor = -1);
        void ReplaceEditorLine(Software::Slate::DocumentService::Document& document,
                               std::vector<std::string>& lines,
                               std::size_t line,
                               const std::string& text,
                               double elapsedSeconds);
        void SplitEditorLine(Software::Slate::DocumentService::Document& document,
                             std::vector<std::string>& lines,
                             double elapsedSeconds);
        void MergeEditorLineWithPrevious(Software::Slate::DocumentService::Document& document,
                                         std::vector<std::string>& lines,
                                         double elapsedSeconds);
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
        void HandleTreeKeys(bool folderPicker);
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
        Software::Slate::LinkService m_links;
        Software::Slate::AssetService m_assets;
        Software::Slate::WorkspaceRegistryService m_workspaceRegistry;
        Software::Slate::NavigationController m_navigation;
        Software::Slate::NavigationController m_searchNavigation;
        Software::Slate::NavigationController m_workspaceNavigation;

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
        bool m_statusIsError = false;
        bool m_searchOverlayOpen = false;
        bool m_filterActive = false;
        bool m_focusPrompt = false;
        bool m_focusFilter = false;
        bool m_folderPickerActive = false;
        bool m_workspaceLoaded = false;
        bool m_editorTextFocused = false;
        bool m_focusEditor = false;
        int m_editorCursorPos = 0;
        int m_editorRequestedCursorPos = -1;
        std::size_t m_editorActiveLine = 0;
        std::size_t m_editorScratchLine = static_cast<std::size_t>(-1);
        std::string m_editorLineText;
        double m_lastScanSeconds = 0.0;
        double m_lastIndexSeconds = 0.0;
    };
}
