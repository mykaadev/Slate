#pragma once

#include "imgui.h"

namespace Software::Core::Runtime
{
    /** Supported input actions. */
    enum class InputAction
    {
        /** Input was pressed or began. */
        Press,
        /** Input was released or ended. */
        Release
    };

    /** Mouse buttons recognized by the framework. */
    enum class MouseButton
    {
        /** Left mouse button. */
        Left,
        /** Right mouse button. */
        Right,
        /** Middle mouse button. */
        Middle
    };

    /** Result values for input handling. */
    enum class InputResult
    {
        /** Event was ignored and should continue bubbling. */
        Ignored,
        /** Event was consumed by the handler. */
        Consumed
    };

    /** Mouse button payload. */
    struct MouseButtonEvent
    {
        /** Button pressed or released. */
        MouseButton button;
        
        /** Action performed on the button. */
        InputAction action;

        /** Mouse position in client coordinates. */
        ImVec2 position;

        /** Modifier mask at the time of the event. */
        int mods;
    };

    /** Mouse movement payload. */
    struct MouseMoveEvent
    {
        /** Current mouse position. */
        ImVec2 position;

        /** Delta since the last event. */
        ImVec2 delta;
    };

    /** Scroll wheel payload. */
    struct ScrollEvent
    {
        /** Scroll offset amount. */
        ImVec2 offset;

        /** Position when the scroll occurred. */
        ImVec2 position;
    };

    /** Keyboard payload. */
    struct KeyEvent
    {
        /** Key code value. */
        int key;

        /** Hardware scan code value. */
        int scancode;

        /** Action performed on the key. */
        InputAction action;

        /** Modifier mask at the time of the event. */
        int mods;
    };
}
