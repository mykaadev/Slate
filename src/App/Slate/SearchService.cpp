#include "App/Slate/SearchService.h"

#include "App/Slate/DocumentService.h"
#include "App/Slate/MarkdownService.h"
#include "App/Slate/PathUtils.h"

#include <algorithm>

namespace Software::Slate
{
    void SearchService::Rebuild(const WorkspaceService& workspace)
    {
        m_files.clear();
        for (const auto& relativePath : workspace.MarkdownFiles())
        {
            std::string text;
            if (!DocumentService::ReadTextFile(workspace.Resolve(relativePath), &text, nullptr))
            {
                continue;
            }

            IndexedFile file;
            file.relativePath = relativePath;
            file.lines = MarkdownService::SplitLines(text);
            file.title = MarkdownService::TitleFromText(text, relativePath);
            file.lowerPath = PathUtils::ToLower(relativePath.generic_string());
            file.lowerTitle = PathUtils::ToLower(file.title);
            file.lowerBody = PathUtils::ToLower(text);
            m_files.push_back(std::move(file));
        }
    }

    std::vector<SearchResult> SearchService::Query(const std::string& query, SearchMode mode, std::size_t limit) const
    {
        std::vector<SearchResult> results;
        const std::string needle = PathUtils::ToLower(PathUtils::Trim(query));
        if (needle.empty())
        {
            return results;
        }

        for (const auto& file : m_files)
        {
            const bool pathMatch = file.lowerPath.find(needle) != std::string::npos;
            const bool titleMatch = file.lowerTitle.find(needle) != std::string::npos;
            int fileScore = 0;
            if (pathMatch)
            {
                fileScore += 10;
            }
            if (titleMatch)
            {
                fileScore += 8;
            }

            if (mode == SearchMode::FileNames)
            {
                if (fileScore > 0)
                {
                    SearchResult result;
                    result.relativePath = file.relativePath;
                    result.path = file.relativePath;
                    result.line = 1;
                    result.column = 1;
                    result.snippet = file.title;
                    result.score = fileScore;
                    results.push_back(std::move(result));
                }
                continue;
            }

            for (std::size_t i = 0; i < file.lines.size(); ++i)
            {
                const std::string lowerLine = PathUtils::ToLower(file.lines[i]);
                const std::size_t pos = lowerLine.find(needle);
                if (pos == std::string::npos)
                {
                    continue;
                }

                SearchResult result;
                result.relativePath = file.relativePath;
                result.path = file.relativePath;
                result.line = i + 1;
                result.column = pos + 1;
                result.snippet = PathUtils::Trim(file.lines[i]);
                result.score = fileScore + (i == 0 ? 8 : 1);
                if (file.lowerTitle.find(needle) != std::string::npos)
                {
                    result.score += 6;
                }
                results.push_back(std::move(result));
            }

            if (fileScore > 0)
            {
                SearchResult result;
                result.relativePath = file.relativePath;
                result.path = file.relativePath;
                result.line = 1;
                result.column = 1;
                result.snippet = file.title;
                result.score = fileScore;
                results.push_back(std::move(result));
            }
        }

        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            if (a.score != b.score)
            {
                return a.score > b.score;
            }
            return a.relativePath.generic_string() < b.relativePath.generic_string();
        });

        if (results.size() > limit)
        {
            results.resize(limit);
        }
        return results;
    }

    std::vector<SearchResult> SearchService::QueryDocument(const std::string& text,
                                                           const std::string& query,
                                                           std::size_t limit)
    {
        std::vector<SearchResult> results;
        const std::string needle = PathUtils::ToLower(PathUtils::Trim(query));
        if (needle.empty())
        {
            return results;
        }

        const auto lines = MarkdownService::SplitLines(text);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const std::string lowerLine = PathUtils::ToLower(lines[i]);
            std::size_t pos = lowerLine.find(needle);
            while (pos != std::string::npos)
            {
                SearchResult result;
                result.line = i + 1;
                result.column = pos + 1;
                result.snippet = PathUtils::Trim(lines[i]);
                result.score = 1;
                results.push_back(std::move(result));
                if (results.size() >= limit)
                {
                    return results;
                }
                pos = lowerLine.find(needle, pos + needle.size());
            }
        }
        return results;
    }
}
