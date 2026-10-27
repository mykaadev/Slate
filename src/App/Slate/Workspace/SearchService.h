#pragma once

#include "App/Slate/Core/SlateTypes.h"
#include "App/Slate/Workspace/WorkspaceService.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Software::Slate
{
    // Builds a lightweight search index for markdown files
    class SearchService
    {
    public:
        // Rebuilds the workspace search index
        void Rebuild(const WorkspaceService& workspace);
        // Queries the workspace index
        std::vector<SearchResult> Query(const std::string& query,
                                        SearchMode mode = SearchMode::FileNames,
                                        std::size_t limit = 100) const;
        // Returns the index revision for query-cache invalidation
        std::uint64_t Revision() const;
        // Queries one document without using the workspace index
        static std::vector<SearchResult> QueryDocument(const std::string& text,
                                                       const std::string& query,
                                                       std::size_t limit = 100);

    private:
        // Stores one indexed markdown file
        struct IndexedFile
        {
            // Stores the relative path for the file
            fs::path relativePath;
            // Stores the display title for the file
            std::string title;
            // Stores the document split into lines
            std::vector<std::string> lines;
            // Stores lowercase lines for full-text matching
            std::vector<std::string> lowerLines;
            // Stores the file timestamp used to reuse the index entry
            fs::file_time_type writeTime{};
            // Stores the file size used to reuse the index entry
            std::uintmax_t fileSize = 0;
            // Stores the lowercase path for matching
            std::string lowerPath;
            // Stores the lowercase title for matching
            std::string lowerTitle;
        };

        // Stores the indexed files
        std::vector<IndexedFile> m_files;
        // Changes when a rebuild changes one or more index entries
        std::uint64_t m_revision = 0;
    };
}
