#pragma once

#include "App/Slate/SlateTypes.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace Software::Slate
{
    enum class TreeViewMode
    {
        Files,
        Folders
    };

    struct TreeViewRow
    {
        fs::path relativePath;
        bool isDirectory = false;
        int depth = 0;
        bool selectable = false;
        bool matchedFilter = false;
    };

    using CollapsedFolderSet = std::unordered_set<std::string>;

    std::vector<TreeViewRow> BuildTreeRows(const std::vector<WorkspaceEntry>& entries,
                                           const CollapsedFolderSet& collapsedFolders,
                                           const std::string& filter,
                                           TreeViewMode mode);

    std::string TreePathKey(const fs::path& relativePath);
    bool PathIsDescendantOf(const fs::path& candidate, const fs::path& folder);
}
