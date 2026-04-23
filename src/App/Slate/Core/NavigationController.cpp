#include "App/Slate/Core/NavigationController.h"

namespace Software::Slate
{
    void NavigationController::SetCount(std::size_t count)
    {
        m_count = count;
        if (m_count == 0)
        {
            m_selected = 0;
        }
        else if (m_selected >= m_count)
        {
            m_selected = m_count - 1;
        }
    }

    void NavigationController::MoveNext()
    {
        if (m_count == 0)
        {
            return;
        }
        m_selected = (m_selected + 1) % m_count;
    }

    void NavigationController::MovePrevious()
    {
        if (m_count == 0)
        {
            return;
        }
        m_selected = (m_selected == 0) ? m_count - 1 : m_selected - 1;
    }

    void NavigationController::Reset()
    {
        m_selected = 0;
    }

    void NavigationController::SetSelected(std::size_t selected)
    {
        if (m_count == 0)
        {
            m_selected = 0;
            return;
        }
        m_selected = selected >= m_count ? m_count - 1 : selected;
    }

    std::size_t NavigationController::Selected() const
    {
        return m_selected;
    }

    bool NavigationController::HasSelection() const
    {
        return m_count > 0;
    }
}
