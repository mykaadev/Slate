#pragma once

#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    // Draws the landing screen for quick entry points
    class SlateHomeMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the home mode
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the home mode
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
    };
}
