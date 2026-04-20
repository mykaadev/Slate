#pragma once

#include "Services/ISettings.h"

#include <unordered_map>

namespace Software::Services
{
    /** In-memory settings store meant for defaults + quick prototyping. */
    class SettingsStore final : public ISettings
    {
        // Functions
    public:
        /** Checks whether a value exists for the given key. */
        bool Has(std::string_view key) const override;

        /** Writes or replaces a value for the given key. */
        void Set(std::string key, SettingValue value) override;

        /** Attempts to retrieve a stored value. */
        std::optional<SettingValue> TryGet(std::string_view key) const override;

        /** Returns a list of stored keys. */
        std::vector<std::string> Keys() const override;

        // Variables
    private:
        /** Internal storage for key/value pairs. */
        std::unordered_map<std::string, SettingValue> m_values;
    };
}
