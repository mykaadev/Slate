#include "Services/ISettings.h"

namespace Software::Services
{
    bool ISettings::GetBool(std::string_view InKey, bool InFallback) const
    {
        auto Value = TryGet(InKey);
        if (!Value) return InFallback;
        if (auto BoolValue = std::get_if<bool>(&*Value)) return *BoolValue;
        return InFallback;
    }

    std::int64_t ISettings::GetInt(std::string_view InKey, std::int64_t InFallback) const
    {
        auto Value = TryGet(InKey);
        if (!Value) return InFallback;
        if (auto IntValue = std::get_if<std::int64_t>(&*Value)) return *IntValue;
        return InFallback;
    }

    double ISettings::GetDouble(std::string_view InKey, double InFallback) const
    {
        auto Value = TryGet(InKey);
        if (!Value) return InFallback;
        if (auto DoubleValue = std::get_if<double>(&*Value)) return *DoubleValue;
        return InFallback;
    }

    std::string ISettings::GetString(std::string_view InKey, std::string InFallback) const
    {
        auto Value = TryGet(InKey);
        if (!Value) return InFallback;
        if (auto StringValue = std::get_if<std::string>(&*Value)) return *StringValue;
        return InFallback;
    }
}
