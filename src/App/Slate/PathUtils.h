#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Software::Slate::PathUtils
{
    namespace fs = std::filesystem;

    std::string ToLower(std::string_view value);
    std::string Trim(std::string_view value);
    std::string GenericString(const fs::path& path);
    std::string SanitizeFileName(std::string_view value);
    std::string StablePathId(const fs::path& path);
    bool IsMarkdownFile(const fs::path& path);
    bool IsImageFile(const fs::path& path);
    bool IsSubPath(const fs::path& root, const fs::path& candidate);
    fs::path NormalizeRelative(const fs::path& path);
    std::string DetectLineEnding(const std::string& text);
    std::string CurrentTimestampForFile();
}
