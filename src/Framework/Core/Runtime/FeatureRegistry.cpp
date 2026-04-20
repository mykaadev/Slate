#include "FeatureRegistry.h"

#include <algorithm>

namespace Software::Core::Runtime
{
    void FeatureRegistry::Register(std::string name, Factory factory, AppContext& context)
    {
        Entry entry;
        entry.name = std::move(name);
        entry.factory = std::move(factory);
        entry.instance = entry.factory();
        if (entry.instance)
        {
            entry.instance->OnRegister(context);
        }
        m_entries.push_back(std::move(entry));
    }

    bool FeatureRegistry::SetEnabled(const std::string& name, bool enabled, AppContext& context)
    {
        Entry* entry = Find(name);
        if (!entry)
        {
            return false;
        }

        if (entry->bEnabled == enabled)
        {
            return true;
        }

        entry->bEnabled = enabled;
        if (entry->instance)
        {
            if (enabled)
            {
                entry->instance->OnEnable(context);
            }
            else
            {
                entry->instance->OnDisable(context);
            }
        }
        return true;
    }

    void FeatureRegistry::Update(AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (entry.bEnabled && entry.instance)
            {
                entry.instance->Update(context);
            }
        }
    }

    void FeatureRegistry::DrawWorld(AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (entry.bEnabled && entry.instance)
            {
                entry.instance->DrawWorld(context);
            }
        }
    }

    void FeatureRegistry::DrawUI(AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (entry.bEnabled && entry.instance)
            {
                entry.instance->DrawUI(context);
            }
        }
    }

    void FeatureRegistry::DrawDetails(AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (entry.bEnabled && entry.instance)
            {
                entry.instance->DrawDetails(context);
            }
        }
    }

    bool FeatureRegistry::DrawDetailsFor(const std::string& name, AppContext& context)
    {
        Entry* entry = Find(name);
        if (!entry || !entry->bEnabled || !entry->instance)
        {
            return false;
        }

        entry->instance->DrawDetails(context);
        return true;
    }

    std::vector<FeatureRegistry::Listing> FeatureRegistry::List() const
    {
        std::vector<Listing> result;
        result.reserve(m_entries.size());
        for (const Entry& entry : m_entries)
        {
            result.push_back(Listing{entry.name, entry.bEnabled});
        }
        return result;
    }

    bool FeatureRegistry::IsEnabled(const std::string& name) const
    {
        const Entry* entry = Find(name);
        return entry ? entry->bEnabled : false;
    }

    InputResult FeatureRegistry::RouteMouseButton(const MouseButtonEvent& event, AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (!entry.bEnabled || !entry.instance)
            {
                continue;
            }

            if (entry.instance->OnMouseButton(event, context) == InputResult::Consumed)
            {
                return InputResult::Consumed;
            }
        }
        return InputResult::Ignored;
    }

    InputResult FeatureRegistry::RouteMouseMove(const MouseMoveEvent& event, AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (!entry.bEnabled || !entry.instance)
            {
                continue;
            }

            if (entry.instance->OnMouseMove(event, context) == InputResult::Consumed)
            {
                return InputResult::Consumed;
            }
        }
        return InputResult::Ignored;
    }

    InputResult FeatureRegistry::RouteScroll(const ScrollEvent& event, AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (!entry.bEnabled || !entry.instance)
            {
                continue;
            }

            if (entry.instance->OnScroll(event, context) == InputResult::Consumed)
            {
                return InputResult::Consumed;
            }
        }
        return InputResult::Ignored;
    }

    InputResult FeatureRegistry::RouteKey(const KeyEvent& event, AppContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (!entry.bEnabled || !entry.instance)
            {
                continue;
            }

            if (entry.instance->OnKey(event, context) == InputResult::Consumed)
            {
                return InputResult::Consumed;
            }
        }
        return InputResult::Ignored;
    }

    FeatureRegistry::Entry* FeatureRegistry::Find(const std::string& name)
    {
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.name == name;
        });
        if (it == m_entries.end())
        {
            return nullptr;
        }
        return &(*it);
    }

    const FeatureRegistry::Entry* FeatureRegistry::Find(const std::string& name) const
    {
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.name == name;
        });
        if (it == m_entries.end())
        {
            return nullptr;
        }
        return &(*it);
    }
}
