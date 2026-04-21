#pragma once

#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    class SlateBrowserMode final : public SlateModeBase
    {
    private:
        const char* ModeName() const override;
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
        void DrawFooterControls(Software::Core::Runtime::AppContext& context) override;

        void PrepareList(Software::Core::Runtime::AppContext& context);
        void PrepareTreeRows(Software::Core::Runtime::AppContext& context, bool folderPicker);
        void OpenSelectedPath(Software::Core::Runtime::AppContext& context);
        void ActivateSelectedTreeRow(Software::Core::Runtime::AppContext& context);
        void ToggleSelectedFolder(Software::Core::Runtime::AppContext& context, bool expanded);
        bool ApplyPendingRenameMove(Software::Core::Runtime::AppContext& context);
        void BeginMovePicker(Software::Core::Runtime::AppContext& context, const Software::Slate::fs::path& relativePath);
        void BeginDelete(Software::Core::Runtime::AppContext& context, const Software::Slate::fs::path& relativePath);

        void HandleListKeys(Software::Core::Runtime::AppContext& context);
        void HandleTreeKeys(Software::Core::Runtime::AppContext& context, bool folderPicker);
        void HandleLibraryKeys(Software::Core::Runtime::AppContext& context);

        void DrawPathList(Software::Core::Runtime::AppContext& context, const char* title, const char* emptyText);
        void DrawFileTree(Software::Core::Runtime::AppContext& context, bool folderPicker);
    };
}
