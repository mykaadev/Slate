#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace Software::Slate
{
    /** Selects which workspace entries should appear in a browser tree. */
    enum class TreeViewMode
    {
        /** Shows folders and markdown files. */
        Files,

        /** Shows folders only. */
        Folders,

        /** Shows non-journal markdown library content only. */
        Library
    };

    /** View-model row rendered by the file tree. */
    struct TreeViewRow
    {
        /** Workspace-relative path represented by the row. */
        fs::path relativePath;

        /** True when the row represents a directory. */
        bool isDirectory = false;

        /** Zero-based visual nesting depth. */
        int depth = 0;

        /** True when the row can be activated by the user. */
        bool selectable = false;

        /** True when this row directly matched the active filter. */
        bool matchedFilter = false;
    };

    /** Set of normalized folder path keys that are currently collapsed. */
    using CollapsedFolderSet = std::unordered_set<std::string>;

    /** Builds browser tree rows from scanned workspace entries, filters, and collapse state. */
    std::vector<TreeViewRow> BuildTreeRows(const std::vector<WorkspaceEntry>& entries,
                                           const CollapsedFolderSet& collapsedFolders,
                                           const std::string& filter,
                                           TreeViewMode mode);

    /** Converts a relative path to the normalized key used by collapsed-folder state. */
    std::string TreePathKey(const fs::path& relativePath);

    /** Returns whether candidate sits below folder in workspace-relative path space. */
    bool PathIsDescendantOf(const fs::path& candidate, const fs::path& folder);
}
