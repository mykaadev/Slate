#pragma once

#include "App/Slate/NavigationController.h"
#include "App/Slate/SlateTypes.h"
#include "App/Slate/WorkspaceService.h"
#include "App/Slate/WorkspaceTree.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    enum class SlateWorkspaceView
    {
        Setup,
        Switcher
    };

    enum class SlateBrowserView
    {
        FileTree,
        Library,
        QuickOpen,
        Recent
    };

    enum class SlateEditorView
    {
        Document,
        Preview,
        Outline
    };

    enum class FolderPickerAction
    {
        None,
        NewNote,
        MoveDestination
    };

    class SlateUiState
    {
    public:
        void ResetForWorkspace(const WorkspaceService& workspace);

        SlateWorkspaceView workspaceView = SlateWorkspaceView::Setup;
        SlateBrowserView browserView = SlateBrowserView::FileTree;
        SlateEditorView editorView = SlateEditorView::Document;
        bool outlinePanelOpen = false;
        bool previewPanelOpen = false;
        float outlinePanelProgress = 0.0f;
        float previewPanelProgress = 0.0f;
        float previewScrollY = 0.0f;
        float previewScrollTargetY = 0.0f;
        double previewManualScrollUntil = 0.0;
        int previewLastFollowLine = -1;
        float nativeEditorScrollLine = 0.0f;
        float nativeEditorScrollTargetLine = 0.0f;
        bool nativeEditorScrollTargetActive = false;

        NavigationController navigation;
        NavigationController workspaceNavigation;
        std::vector<fs::path> documentHistory;
        int documentHistoryIndex = -1;

        fs::path pendingPath;
        fs::path pendingTargetPath;
        fs::path pendingFolderPath = ".";
        RewritePlan pendingRewritePlan;
        std::string pendingWorkspaceTitle;
        std::string pendingWorkspaceEmoji;

        std::vector<fs::path> visiblePaths;
        std::vector<TreeViewRow> treeRows;
        std::vector<Heading> visibleHeadings;
        CollapsedFolderSet collapsedFolders;

        std::string filterText;
        bool filterActive = false;
        bool focusFilter = false;
        bool folderPickerActive = false;
        FolderPickerAction folderPickerAction = FolderPickerAction::None;
    };
}
