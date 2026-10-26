#pragma once

#include "Modes/Slate/SlateModeBase.h"

#include <string>

namespace Software::Modes::Slate
{
    // Hosts a full-screen view contributed by a plugin, such as a kanban board
    class SlateViewMode final : public SlateModeBase
    {
    private:
        // Returns the mode id
        const char* ModeName() const override;
        // Draws the active view inside the Slate shell
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        // Returns helper text for the view screen
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;
        // The view title in capitals, for example KANBAN
        std::string StatusBadge(const Software::Core::Runtime::AppContext& context) const override;
    };
}
