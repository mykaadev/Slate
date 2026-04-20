#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Software::Slate
{
    namespace fs = std::filesystem;

    struct WorkspaceEntry
    {
        fs::path path;
        fs::path relativePath;
        bool isDirectory = false;
        int depth = 0;
    };

    struct Heading
    {
        int level = 1;
        std::size_t line = 0;
        std::string title;
    };

    struct MarkdownLink
    {
        std::size_t line = 0;
        std::size_t start = 0;
        std::size_t length = 0;
        std::string label;
        std::string target;
        bool isWiki = false;
        bool isImage = false;
    };

    struct MarkdownSummary
    {
        bool hasFrontmatter = false;
        std::size_t frontmatterEndLine = 0;
        std::vector<Heading> headings;
        std::vector<MarkdownLink> links;
    };

    struct SearchResult
    {
        fs::path path;
        fs::path relativePath;
        std::size_t line = 0;
        std::string snippet;
        int score = 0;
    };

    struct RewriteChange
    {
        fs::path path;
        fs::path relativePath;
        std::string originalText;
        std::string updatedText;
        int replacementCount = 0;
    };

    struct RewritePlan
    {
        fs::path oldRelativePath;
        fs::path newRelativePath;
        std::vector<RewriteChange> changes;

        int TotalReplacements() const
        {
            int total = 0;
            for (const auto& change : changes)
            {
                total += change.replacementCount;
            }
            return total;
        }
    };

    struct WorkspaceVault
    {
        std::string id;
        std::string emoji;
        std::string title;
        fs::path path;
    };
}
