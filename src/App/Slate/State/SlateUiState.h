#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "App/Slate/Workspace/WorkspaceService.h"
#include "App/Slate/Workspace/WorkspaceTree.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    // Lists the workspace level screens
    enum class SlateWorkspaceView
    {
        Setup,
        Switcher
    };

    // Lists the browser views
    enum class SlateBrowserView
    {
        FileTree,
        Library,
        QuickOpen,
        Recent
    };

    // Lists the editor companion panels
    enum class SlateEditorView
    {
        Document,
        Preview,
        Outline
    };

    // Lists actions that reuse the folder picker
    enum class FolderPickerAction
    {
        None,
        NewNote,
        MoveDestination
    };

    // Stores transient UI state shared across modes
    class SlateUiState
    {
    public:
        // Resets UI state after switching workspace
        void ResetForWorkspace(const WorkspaceService& workspace);

        // Stores the current workspace screen
        SlateWorkspaceView workspaceView = SlateWorkspaceView::Setup;
        // Stores the current browser screen
        SlateBrowserView browserView = SlateBrowserView::FileTree;
        // Stores the current editor companion panel
        SlateEditorView editorView = SlateEditorView::Document;
        // Marks whether the outline panel should be visible
        bool outlinePanelOpen = false;
        // Marks whether the preview panel should be visible
        bool previewPanelOpen = false;
        // Stores the outline reveal animation progress
        float outlinePanelProgress = 0.0f;
        // Stores the preview reveal animation progress
        float previewPanelProgress = 0.0f;
        // Stores the current preview scroll position
        float previewScrollY = 0.0f;
        // Stores the target preview scroll position
        float previewScrollTargetY = 0.0f;
        // Stores how long manual preview scrolling wins over follow mode
        double previewManualScrollUntil = 0.0;
        // Stores the last source line used for preview follow
        int previewLastFollowLine = -1;
        // Stores the current native editor top line
        float nativeEditorScrollLine = 0.0f;
        // Stores the target native editor top line
        float nativeEditorScrollTargetLine = 0.0f;
        // Marks whether a native editor scroll target is active
        bool nativeEditorScrollTargetActive = false;

        // Tracks selection in the main browser list
        NavigationController navigation;
        // Tracks selection in the workspace switcher
        NavigationController workspaceNavigation;
        // Stores document history for back and forward moves
        std::vector<fs::path> documentHistory;
        // Stores the active document history index
        int documentHistoryIndex = -1;

        // Stores a path waiting for the next command
        fs::path pendingPath;
        // Stores a move target path
        fs::path pendingTargetPath;
        // Stores the default folder for new items
        fs::path pendingFolderPath = ".";
        // Stores the pending rewrite plan for rename operations
        RewritePlan pendingRewritePlan;
        // Stores the title for workspace creation
        std::string pendingWorkspaceTitle;
        // Stores the emoji for workspace creation
        std::string pendingWorkspaceEmoji;

        // Stores visible paths for flat browser views
        std::vector<fs::path> visiblePaths;
        // Stores visible rows for tree browser views
        std::vector<TreeViewRow> treeRows;
        // Stores visible headings for the outline
        std::vector<Heading> visibleHeadings;
        // Stores collapsed folders in the tree
        CollapsedFolderSet collapsedFolders;

        // Stores the active browser filter text
        std::string filterText;
        // Marks whether the browser filter owns focus
        bool filterActive = false;
        // Requests focus for the browser filter
        bool focusFilter = false;
        // Marks whether the folder picker is active
        bool folderPickerActive = false;
        // Stores the current folder picker action
        FolderPickerAction folderPickerAction = FolderPickerAction::None;
    };
}
