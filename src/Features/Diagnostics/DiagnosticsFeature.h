#pragma once

#include "Core/Runtime/Feature.h"

#include <string>

namespace Software::Features::Diagnostics
{
    /** Feature that exposes diagnostics windows and input visualization. */
    class DiagnosticsFeature final : public Software::Core::Runtime::Feature
    {
    // Functions
    public:

        /** Enables diagnostic overlays and initializes state. */
        void OnEnable(Software::Core::Runtime::AppContext& context) override;

        /** Draws the diagnostics UI content. */
        void DrawUI(Software::Core::Runtime::AppContext& context) override;

        /** Draws details panels for diagnostic data. */
        void DrawDetails(Software::Core::Runtime::AppContext& context) override;

        /** Handles keyboard input and logs last key state. */
        Software::Core::Runtime::InputResult OnKey(const Software::Core::Runtime::KeyEvent& event,
                                                 Software::Core::Runtime::AppContext& context) override;

        /** Handles mouse movement and records cursor position. */
        Software::Core::Runtime::InputResult OnMouseMove(const Software::Core::Runtime::MouseMoveEvent& event,
                                                       Software::Core::Runtime::AppContext& context) override;

    // Variables
    private:
    
        /** Whether to display the diagnostics window. */
        bool bShowWindow = true;

        /** Whether to display the ImGui demo window. */
        bool bShowImGuiDemo = false;

        /** Whether to display the log window. */
        bool bShowLog = true;

        /** Last received key code. */
        int m_lastKey = 0;

        /** Last received key action. */
        int m_lastKeyAction = 0;

        /** Last recorded mouse X position. */
        float m_lastMouseX = 0.0f;

        /** Last recorded mouse Y position. */
        float m_lastMouseY = 0.0f;

        /** Current diagnostic status line. */
        std::string m_statusLine;
    };
}
