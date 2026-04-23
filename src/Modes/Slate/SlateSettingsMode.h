#pragma once

#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    // Draws the compact settings and theme screen
    class SlateSettingsMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the settings mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the settings mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
    };
}
