#pragma once

#include "imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate::SettingsFile
{
    std::vector<std::string> SplitTabs(const std::string& line);
    std::string Escape(std::string_view value);
    std::string Unescape(std::string_view value);

    bool ParseBool(std::string_view value, bool* out);
    bool ParseInt(std::string_view value, int* out);
    bool ParseColor(std::string_view value, ImVec4* out);

    std::string SerializeBool(bool value);
    std::string SerializeInt(int value);
    std::string SerializeColor(const ImVec4& value);
}
