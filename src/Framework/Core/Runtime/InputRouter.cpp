#include "InputRouter.h"

#include "Core/Runtime/ToolRegistry.h"

namespace Software::Core::Runtime
{
    void InputRouter::Bind(ToolRegistry* tools)
    {
        m_tools = tools;
    }

    void InputRouter::SetContext(AppContext* context)
    {
        m_context = context;
    }

    void InputRouter::OnMouseButton(const MouseButtonEvent& event)
    {
        if (!m_context)
        {
            return;
        }

        if (m_tools && m_tools->RouteMouseButton(event, *m_context) == InputResult::Consumed)
        {
            return;
        }
    }

    void InputRouter::OnMouseMove(const MouseMoveEvent& event)
    {
        if (!m_context)
        {
            return;
        }

        if (m_tools && m_tools->RouteMouseMove(event, *m_context) == InputResult::Consumed)
        {
            return;
        }
    }

    void InputRouter::OnScroll(const ScrollEvent& event)
    {
        if (!m_context)
        {
            return;
        }

        if (m_tools && m_tools->RouteScroll(event, *m_context) == InputResult::Consumed)
        {
            return;
        }
    }

    void InputRouter::OnKey(const KeyEvent& event)
    {
        if (!m_context)
        {
            return;
        }

        if (m_tools && m_tools->RouteKey(event, *m_context) == InputResult::Consumed)
        {
            return;
        }
    }
}
