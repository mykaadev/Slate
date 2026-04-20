#pragma once

#include "Core/Runtime/Mode.h"

namespace Software::Modes::Example
{
    /** Minimal example mode/tool to demonstrate the ToolRegistry contract. */
    class ExampleMode final : public Software::Core::Runtime::Mode
    {
        // Functions
    public:
        /** Handles activation of the mode. */
        void OnEnter(Software::Core::Runtime::AppContext& InContext) override;

        /** Handles deactivation of the mode. */
        void OnExit(Software::Core::Runtime::AppContext& InContext) override;

        /** Draws the main UI for the mode. */
        void DrawUI(Software::Core::Runtime::AppContext& InContext) override;

        /** Draws the details panel content. */
        void DrawDetails(Software::Core::Runtime::AppContext& InContext) override;

        /** Reacts to mouse button events. */
        Software::Core::Runtime::InputResult OnMouseButton(const Software::Core::Runtime::MouseButtonEvent& InEvent,
                                                           Software::Core::Runtime::AppContext& InContext) override;

        // Variables
    private:
        /** Number of clicks recorded by the mode. */
        std::int64_t m_clicks = 0;

        /** Controls visibility of the example window. */
        bool m_bShowWindow = true;
    };
}
