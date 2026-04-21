#pragma once

#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    class SlateSettingsMode final : public SlateModeBase
    {
    private:
        const char* ModeName() const override;
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
    };
}
