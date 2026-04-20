#pragma once

#include <memory>

#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/InputEvents.h"

namespace Software::Core::Runtime
{
    /** Base class describing the behaviour contract for exclusive modes/tools. */
    class Mode
    {
        // Functions
    public:
        /** Virtual destructor for interface safety. */
        virtual ~Mode() = default;

        /** Called when the mode becomes active. */
        virtual void OnEnter(AppContext& InContext)
        {
        }

        /** Called when the mode is deactivated. */
        virtual void OnExit(AppContext& InContext)
        {
        }

        /** Performs per-frame updates. */
        virtual void Update(AppContext& InContext)
        {
        }

        /** Renders world content. */
        virtual void DrawWorld(AppContext& InContext)
        {
        }

        /** Renders UI overlays. */
        virtual void DrawUI(AppContext& InContext)
        {
        }

        /** Renders details panel content. */
        virtual void DrawDetails(AppContext& InContext)
        {
        }

        /** Handles mouse button events. */
        virtual InputResult OnMouseButton(const MouseButtonEvent& InEvent, AppContext& InContext)
        {
            return InputResult::Ignored;
        }

        /** Handles mouse movement. */
        virtual InputResult OnMouseMove(const MouseMoveEvent& InEvent, AppContext& InContext)
        {
            return InputResult::Ignored;
        }

        /** Handles scroll wheel input. */
        virtual InputResult OnScroll(const ScrollEvent& InEvent, AppContext& InContext)
        {
            return InputResult::Ignored;
        }

        /** Handles keyboard events. */
        virtual InputResult OnKey(const KeyEvent& InEvent, AppContext& InContext)
        {
            return InputResult::Ignored;
        }
    };

    using ModePtr = std::unique_ptr<Mode>;
}
