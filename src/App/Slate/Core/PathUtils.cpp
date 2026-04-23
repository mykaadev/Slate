#include "App/Slate/Core/PathUtils.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <functional>
#include <iomanip>
#include <sstream>

namespace Software::Slate::PathUtils
{
    std::string ToLower(std::string_view value)
    {
        std::string out(value);
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return out;
    }

    std::string Trim(std::string_view value)
    {
        std::size_t first = 0;
        while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
        {
            ++first;
        }

        std::size_t last = value.size();
        while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
        {
            --last;
        }

        return std::string(value.substr(first, last - first));
    }

    std::string GenericString(const fs::path& path)
    {
        return path.generic_string();
    }

    std::string SanitizeFileName(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for (const char ch : value)
        {
            const bool safe = std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.';
            out.push_back(safe ? ch : '-');
        }

        while (!out.empty() && (out.front() == '.' || out.front() == '-'))
        {
            out.erase(out.begin());
        }
        while (!out.empty() && out.back() == '-')
        {
            out.pop_back();
        }

        return out.empty() ? "untitled" : out;
    }

    std::string StablePathId(const fs::path& path)
    {
        const std::string text = ToLower(path.lexically_normal().generic_string());
        const std::size_t hash = std::hash<std::string>{}(text);
        std::ostringstream ss;
        ss << std::hex << hash;
        return ss.str();
    }

    bool IsMarkdownFile(const fs::path& path)
    {
        const std::string ext = ToLower(path.extension().string());
        return ext == ".md" || ext == ".markdown";
    }

    bool IsImageFile(const fs::path& path)
    {
        const std::string ext = ToLower(path.extension().string());
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp" ||
               ext == ".webp";
    }

    bool IsSubPath(const fs::path& root, const fs::path& candidate)
    {
        std::error_code ec;
        const fs::path canonicalRoot = fs::weakly_canonical(root, ec);
        if (ec)
        {
            return false;
        }

        const fs::path canonicalCandidate = fs::weakly_canonical(candidate, ec);
        if (ec)
        {
            return false;
        }

        auto rootIt = canonicalRoot.begin();
        auto candidateIt = canonicalCandidate.begin();
        for (; rootIt != canonicalRoot.end(); ++rootIt, ++candidateIt)
        {
            if (candidateIt == canonicalCandidate.end() || *rootIt != *candidateIt)
            {
                return false;
            }
        }
        return true;
    }

    fs::path NormalizeRelative(const fs::path& path)
    {
        fs::path out;
        for (const auto& part : path.lexically_normal())
        {
            if (part == ".")
            {
                continue;
            }
            out /= part;
        }
        return out;
    }

    std::string DetectLineEnding(const std::string& text)
    {
        const std::size_t crlf = text.find("\r\n");
        if (crlf != std::string::npos)
        {
            return "\r\n";
        }
        return "\n";
    }

    std::string CurrentTimestampForFile()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#if defined(_WIN32)
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif

        std::ostringstream ss;
        ss << std::put_time(&localTime, "%Y%m%d_%H%M%S");
        return ss.str();
    }
}


