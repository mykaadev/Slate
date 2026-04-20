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
        std::vector<SearchResult> Query(const std::string& query, std::size_t limit = 100) const;

    private:
        struct IndexedFile
        {
            fs::path relativePath;
            std::string title;
            std::vector<std::string> lines;
            std::string lowerPath;
            std::string lowerBody;
        };

        std::vector<IndexedFile> m_files;
    };
}
