#pragma once

#include "App/Slate/SlateTypes.h"
#include "App/Slate/WorkspaceService.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    class SearchService
    {
    public:
        void Rebuild(const WorkspaceService& workspace);
        std::vector<SearchResult> Query(const std::string& query,
                                        SearchMode mode = SearchMode::FileNames,
                                        std::size_t limit = 100) const;
        static std::vector<SearchResult> QueryDocument(const std::string& text,
                                                       const std::string& query,
                                                       std::size_t limit = 100);

    private:
        struct IndexedFile
        {
            fs::path relativePath;
            std::string title;
            std::vector<std::string> lines;
            std::string lowerPath;
            std::string lowerTitle;
            std::string lowerBody;
        };

        std::vector<IndexedFile> m_files;
    };
}
