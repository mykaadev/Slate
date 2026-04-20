#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Software::Services
{
    using SettingValue = std::variant<bool, std::int64_t, double, std::string>;

    /** Interface describing a simple key/value settings store. */
    class ISettings
    {
        // Functions
    public:
        /** Virtual destructor for interface safety. */
        virtual ~ISettings() = default;

        /** Returns true when a key exists in the store. */
        virtual bool Has(std::string_view InKey) const = 0;

        /** Assigns a value to the provided key. */
        virtual void Set(std::string InKey, SettingValue InValue) = 0;

        /** Attempts to retrieve a key and returns an optional value. */
        virtual std::optional<SettingValue> TryGet(std::string_view InKey) const = 0;

        /** Lists every key tracked by the store. */
        virtual std::vector<std::string> Keys() const = 0;

        /** Convenience helper to read a boolean with a fallback. */
        bool GetBool(std::string_view InKey, bool InFallback = false) const;

        /** Convenience helper to read an integer with a fallback. */
        std::int64_t GetInt(std::string_view InKey, std::int64_t InFallback = 0) const;

        /** Convenience helper to read a double with a fallback. */
        double GetDouble(std::string_view InKey, double InFallback = 0.0) const;

        /** Convenience helper to read a string with a fallback. */
        std::string GetString(std::string_view InKey, std::string InFallback = {}) const;
    };

    using ISettingsPtr = std::shared_ptr<ISettings>;
}
