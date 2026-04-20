#include "ToolRegistry.h"

#include <algorithm>

namespace Software::Core::Runtime
{
    void ToolRegistry::Register(std::string name, Factory factory)
    {
        Entry entry;
        entry.name = std::move(name);
        entry.factory = std::move(factory);
        m_entries.push_back(std::move(entry));
    }

    bool ToolRegistry::Activate(const std::string& name, AppContext& context)
    {
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.name == name;
        });
        if (it == m_entries.end())
        {
            return false;
        }

        Entry& entry = *it;
        if (m_activeEntry == &entry)
        {
            return true;
        }

        if (m_activeEntry && m_activeEntry->instance)
        {
            m_activeEntry->instance->OnExit(context);
        }

        if (!entry.instance)
        {
            entry.instance = entry.factory ? entry.factory() : nullptr;
        }

        m_activeEntry = nullptr;
        m_activeName.clear();

        if (entry.instance)
        {
            m_activeEntry = &entry;
            m_activeName = entry.name;
            entry.instance->OnEnter(context);
        }

        return entry.instance != nullptr;
    }

    void ToolRegistry::Update(AppContext& context)
    {
        if (m_activeEntry && m_activeEntry->instance)
        {
            m_activeEntry->instance->Update(context);
        }
    }

    void ToolRegistry::DrawWorld(AppContext& context)
    {
        if (m_activeEntry && m_activeEntry->instance)
        {
            m_activeEntry->instance->DrawWorld(context);
        }
    }

    void ToolRegistry::DrawUI(AppContext& context)
    {
        if (m_activeEntry && m_activeEntry->instance)
        {
            m_activeEntry->instance->DrawUI(context);
        }
    }

    void ToolRegistry::DrawDetails(AppContext& context)
    {
        if (m_activeEntry && m_activeEntry->instance)
        {
            m_activeEntry->instance->DrawDetails(context);
        }
    }

    Mode* ToolRegistry::Active() const
    {
        if (!m_activeEntry)
        {
            return nullptr;
        }
        return m_activeEntry->instance.get();
    }

    const std::string& ToolRegistry::ActiveName() const
    {
        return m_activeName;
    }

    std::vector<ToolRegistry::Listing> ToolRegistry::List() const
    {
        std::vector<Listing> result;
        result.reserve(m_entries.size());
        for (const Entry& entry : m_entries)
        {
            result.push_back(Listing{entry.name, &entry == m_activeEntry});
        }
        return result;
    }

    InputResult ToolRegistry::RouteMouseButton(const MouseButtonEvent& event, AppContext& context)
    {
        if (!m_activeEntry || !m_activeEntry->instance)
        {
            return InputResult::Ignored;
        }
        return m_activeEntry->instance->OnMouseButton(event, context);
    }

    InputResult ToolRegistry::RouteMouseMove(const MouseMoveEvent& event, AppContext& context)
    {
        if (!m_activeEntry || !m_activeEntry->instance)
        {
            return InputResult::Ignored;
        }
        return m_activeEntry->instance->OnMouseMove(event, context);
    }

    InputResult ToolRegistry::RouteScroll(const ScrollEvent& event, AppContext& context)
    {
        if (!m_activeEntry || !m_activeEntry->instance)
        {
            return InputResult::Ignored;
        }
        return m_activeEntry->instance->OnScroll(event, context);
    }

    InputResult ToolRegistry::RouteKey(const KeyEvent& event, AppContext& context)
    {
        if (!m_activeEntry || !m_activeEntry->instance)
        {
            return InputResult::Ignored;
        }
        return m_activeEntry->instance->OnKey(event, context);
    }
}
