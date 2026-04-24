#pragma once

#include "Core/Runtime/InputEvents.h"

namespace Software::Core::Runtime
{
    class ToolRegistry;
    struct AppContext;

    /** Dispatches raw input events to the active mode/tool. */
    class InputRouter
    {
    // Functions
    public:

        /** Binds registries for dispatch. */
        void Bind(ToolRegistry* tools);
        
        /** Updates the shared application context pointer. */
        void SetContext(AppContext* context);

        /** Dispatches a mouse button event. */
        void OnMouseButton(const MouseButtonEvent& event);

        /** Dispatches a mouse movement event. */
        void OnMouseMove(const MouseMoveEvent& event);

        /** Dispatches a scroll event. */
        void OnScroll(const ScrollEvent& event);

        /** Dispatches a keyboard event. */
        void OnKey(const KeyEvent& event);

    // Variables
    private:

        /** Active tool registry used for mode input. */
        ToolRegistry* m_tools = nullptr;

        /** Current application context. */
        AppContext* m_context = nullptr;
    };
}
