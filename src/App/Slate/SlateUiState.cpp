#include "App/Slate/SlateUiState.h"

namespace Software::Slate
{
    void SlateUiState::ResetForWorkspace(const WorkspaceService& workspace)
    {
        workspaceView = SlateWorkspaceView::Switcher;
        browserView = SlateBrowserView::FileTree;
        editorView = SlateEditorView::Document;

        navigation.Reset();
        workspaceNavigation.Reset();

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
