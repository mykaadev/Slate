#include "Services/SettingsStore.h"

#include <algorithm>

namespace Software::Services
{
    bool SettingsStore::Has(std::string_view key) const
    {
        return m_values.find(std::string(key)) != m_values.end();
    }

    void SettingsStore::Set(std::string key, SettingValue value)
    {
        m_values[std::move(key)] = std::move(value);
    }

    std::optional<SettingValue> SettingsStore::TryGet(std::string_view key) const
    {
        auto it = m_values.find(std::string(key));
        if (it == m_values.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<std::string> SettingsStore::Keys() const
    {
        std::vector<std::string> keys;
        keys.reserve(m_values.size());
        for (const auto& kv : m_values)
        {
            keys.push_back(kv.first);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }
}
