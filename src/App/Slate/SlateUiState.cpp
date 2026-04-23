#include "App/Slate/SlateUiState.h"

namespace Software::Slate
{
    void SlateUiState::ResetForWorkspace(const WorkspaceService& workspace)
    {
        workspaceView = SlateWorkspaceView::Switcher;
        browserView = SlateBrowserView::FileTree;
        editorView = SlateEditorView::Document;
        outlinePanelOpen = false;
        previewPanelOpen = false;
        outlinePanelProgress = 0.0f;
        previewPanelProgress = 0.0f;
        previewScrollY = 0.0f;
        previewScrollTargetY = 0.0f;
        previewManualScrollUntil = 0.0;
        previewLastFollowLine = -1;
        nativeEditorScrollLine = 0.0f;
        nativeEditorScrollTargetLine = 0.0f;
        nativeEditorScrollTargetActive = false;

        navigation.Reset();
        workspaceNavigation.Reset();
        documentHistory.clear();
        documentHistoryIndex = -1;

        pendingPath.clear();
        pendingTargetPath.clear();
        pendingFolderPath = ".";
        pendingRewritePlan = {};
        pendingWorkspaceTitle.clear();
        pendingWorkspaceEmoji.clear();

        visiblePaths.clear();
        treeRows.clear();
        visibleHeadings.clear();

        collapsedFolders.clear();
        for (const auto& entry : workspace.Entries())
        {
            if (entry.isDirectory)
            {
                collapsedFolders.insert(TreePathKey(entry.relativePath));
            }
        }

        filterText.clear();
        filterActive = false;
        focusFilter = false;
        folderPickerActive = false;
        folderPickerAction = FolderPickerAction::None;
    }
}
