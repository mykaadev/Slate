#pragma once

#include "App/Slate/Editor/Live/LiveEditor.h"
#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    // Draws file tree library and recent document views
    class SlateBrowserMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the browser mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the browser mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
        // Draws browser specific footer content
        void DrawFooterControls(Software::Core::Runtime::AppContext& context) override;

        // Prepares the flat visible path list
        void PrepareList(Software::Core::Runtime::AppContext& context);
        // Prepares tree rows for the current browser view
        void PrepareTreeRows(Software::Core::Runtime::AppContext& context, bool folderPicker);
        // Opens the selected list path
        void OpenSelectedPath(Software::Core::Runtime::AppContext& context);
        // Activates the selected tree row
        void ActivateSelectedTreeRow(Software::Core::Runtime::AppContext& context);
        // Expands or collapses the selected folder
        void ToggleSelectedFolder(Software::Core::Runtime::AppContext& context, bool expanded);
        // Applies the pending rename or move action
        bool ApplyPendingRenameMove(Software::Core::Runtime::AppContext& context);
        // Starts the move destination picker
        void BeginMovePicker(Software::Core::Runtime::AppContext& context, const Software::Slate::fs::path& relativePath);
        // Starts delete confirmation for one path
        void BeginDelete(Software::Core::Runtime::AppContext& context, const Software::Slate::fs::path& relativePath);

        // Handles flat list shortcuts
        void HandleListKeys(Software::Core::Runtime::AppContext& context);
        // Handles tree view shortcuts
        void HandleTreeKeys(Software::Core::Runtime::AppContext& context, bool folderPicker);
        // Handles library shortcuts
        void HandleLibraryKeys(Software::Core::Runtime::AppContext& context);

        // Draws a flat path list
        void DrawPathList(Software::Core::Runtime::AppContext& context, const char* title, const char* emptyText);
        // Draws the file tree view
        void DrawFileTree(Software::Core::Runtime::AppContext& context, bool folderPicker);
        // Draws a rendered preview of the selected note or a folder summary
        void DrawPreview(Software::Core::Runtime::AppContext& context, float width, float height);

        // Read-only live editor used for the preview pane
        Software::Slate::Live::LiveEditor m_preview;
        // Stores the note shown in the preview and when it was last read
        Software::Slate::fs::path m_previewPath;
        double m_previewLoadedAt = -10.0;
    };
}
