#pragma once

#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    // Draws workspace setup and switching flows
    class SlateWorkspaceMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the workspace mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the workspace mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;

        // Starts workspace creation
        void BeginWorkspaceCreate(Software::Core::Runtime::AppContext& context);
        // Draws the workspace setup body
        void DrawWorkspaceSetup(Software::Core::Runtime::AppContext& context);
        // Draws the workspace switcher body
        void DrawWorkspaceSwitcher(Software::Core::Runtime::AppContext& context);
    };
}
