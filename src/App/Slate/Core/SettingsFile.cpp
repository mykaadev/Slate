#include "App/Slate/Core/SettingsFile.h"

#include "App/Slate/Core/PathUtils.h"

#include <array>
#include <charconv>
#include <cstdlib>
#include <sstream>

namespace Software::Slate::SettingsFile
{
    std::vector<std::string> SplitTabs(const std::string& line)
    {
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (start <= line.size())
        {
            const std::size_t end = line.find('\t', start);
            if (end == std::string::npos)
            {
                parts.push_back(line.substr(start));
                break;
            }

            parts.push_back(line.substr(start, end - start));
            start = end + 1;
        }
        return parts;
    }

    std::string Escape(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                out += "\\\\";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out.push_back(ch);
                break;
            }
        }
        return out;
    }

    std::string Unescape(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '\\' || i + 1 >= value.size())
            {
                out.push_back(value[i]);
                continue;
            }

            const char next = value[++i];
            switch (next)
            {
            case 't':
                out.push_back('\t');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            default:
                out.push_back(next);
                break;
            }
        }
        return out;
    }

    bool ParseBool(std::string_view value, bool* out)
    {
        const std::string lower = PathUtils::ToLower(PathUtils::Trim(value));
        if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
        {
            if (out)
            {
                *out = true;
            }
            return true;
        }
        if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
        {
            if (out)
            {
                *out = false;
            }
            return true;
        }
        return false;
    }

    bool ParseInt(std::string_view value, int* out)
    {
        const std::string trimmed = PathUtils::Trim(value);
        if (trimmed.empty())
        {
            return false;
        }

        int parsed = 0;
        const auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed);
        if (result.ec != std::errc() || result.ptr != trimmed.data() + trimmed.size())
        {
            return false;
        }

        if (out)
        {
            *out = parsed;
        }
        return true;
    }

    bool ParseColor(std::string_view value, ImVec4* out)
    {
        std::array<float, 4> components{0.0f, 0.0f, 0.0f, 1.0f};
        std::stringstream ss(PathUtils::Trim(value));
        std::string token;
        int index = 0;
        while (std::getline(ss, token, ',') && index < static_cast<int>(components.size()))
        {
            char* end = nullptr;
            const double parsed = std::strtod(token.c_str(), &end);
            if (end == token.c_str())
            {
                return false;
            }
            components[static_cast<std::size_t>(index++)] = static_cast<float>(parsed);
        }

        if (index < 3)
        {
            return false;
        }

        if (out)
        {
            *out = ImVec4(components[0], components[1], components[2],
                          index >= 4 ? components[3] : 1.0f);
        }
        return true;
    }

    std::string SerializeBool(bool value)
    {
        return value ? "true" : "false";
    }

    std::string SerializeInt(int value)
    {
        return std::to_string(value);
    }

    std::string SerializeColor(const ImVec4& value)
    {
        std::ostringstream ss;
        ss.setf(std::ios::fixed, std::ios::floatfield);
        ss.precision(3);
        ss << value.x << "," << value.y << "," << value.z << "," << value.w;
        return ss.str();
    }
}
