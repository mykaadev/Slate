#pragma once

#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    class SlateWorkspaceMode final : public SlateModeBase
    {
    private:
        const char* ModeName() const override;
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;

        void BeginWorkspaceCreate(Software::Core::Runtime::AppContext& context);
        void DrawWorkspaceSetup();
        void DrawWorkspaceSwitcher(Software::Core::Runtime::AppContext& context);
    };
}
