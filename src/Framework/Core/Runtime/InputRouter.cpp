#include "InputRouter.h"

#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/ToolRegistry.h"

namespace Software::Core::Runtime
{
    void InputRouter::Bind(ToolRegistry* tools, FeatureRegistry* features)
    {
        m_tools = tools;
        m_features = features;
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

        if (m_features)
        {
            m_features->RouteMouseButton(event, *m_context);
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

        if (m_features)
        {
            m_features->RouteMouseMove(event, *m_context);
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

        if (m_features)
        {
            m_features->RouteScroll(event, *m_context);
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

        if (m_features)
        {
            m_features->RouteKey(event, *m_context);
        }
    }
}
