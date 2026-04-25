#pragma once

#include "Modes/Slate/SlateModeBase.h"
#include "App/Slate/Input/ShortcutService.h"

#include <string>

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

        bool m_shortcutCaptureOpen = false;
        bool m_shortcutCaptureArmed = false;
        Software::Slate::ShortcutAction m_shortcutCaptureAction = Software::Slate::ShortcutAction::Count;
        std::string m_shortcutCaptureLabel;
    };
}
