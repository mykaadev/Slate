#include "App/Slate/WorkspaceTree.h"

#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        bool IsJournalPath(const fs::path& path)
        {
            const std::string text = PathUtils::ToLower(PathUtils::NormalizeRelative(path).generic_string());
            return text == "journal" || text.rfind("journal/", 0) == 0;
        }

        bool IsLibraryMarkdownFile(const WorkspaceEntry& entry)
        {
            return !entry.isDirectory && !IsJournalPath(entry.relativePath) &&
                   PathUtils::IsMarkdownFile(entry.relativePath);
        }

        bool HasLibraryDescendant(const std::vector<WorkspaceEntry>& entries, const fs::path& folder)
        {
            for (const auto& entry : entries)
            {
                if (IsLibraryMarkdownFile(entry) && PathIsDescendantOf(entry.relativePath, folder))
                {
                    return true;
                }
            }
            return false;
        }

        bool IsVisibleEntry(const std::vector<WorkspaceEntry>& entries, const WorkspaceEntry& entry, TreeViewMode mode)
        {
            if (mode == TreeViewMode::Folders)
            {
                return entry.isDirectory;
            }

            if (mode == TreeViewMode::Library)
            {
                if (entry.isDirectory)
                {
                    return !IsJournalPath(entry.relativePath) && HasLibraryDescendant(entries, entry.relativePath);
                }
                return IsLibraryMarkdownFile(entry);
            }

            return entry.isDirectory || PathUtils::IsMarkdownFile(entry.relativePath);
        }

        bool HasCollapsedAncestor(const fs::path& path, const CollapsedFolderSet& collapsedFolders)
        {
            fs::path parent = path.parent_path();
            while (!parent.empty() && parent != ".")
            {
                if (collapsedFolders.find(TreePathKey(parent)) != collapsedFolders.end())
                {
                    return true;
                }
                parent = parent.parent_path();
            }
            return false;
        }
    }

    std::vector<TreeViewRow> BuildTreeRows(const std::vector<WorkspaceEntry>& entries,
                                           const CollapsedFolderSet& collapsedFolders,
                                           const std::string& filter,
                                           TreeViewMode mode)
    {
        std::vector<TreeViewRow> rows;
        const std::string needle = PathUtils::ToLower(PathUtils::Trim(filter));
        const bool filtering = !needle.empty();

        for (const auto& entry : entries)
        {
            if (!IsVisibleEntry(entries, entry, mode))
            {
                continue;
            }

            const bool selfMatches =
                !filtering || PathUtils::ToLower(entry.relativePath.generic_string()).find(needle) != std::string::npos;

            bool descendantMatches = false;
            if (filtering && entry.isDirectory)
            {
                for (const auto& other : entries)
                {
                    if (&other == &entry || !IsVisibleEntry(entries, other, mode))
                    {
                        continue;
                    }

                    if (PathIsDescendantOf(other.relativePath, entry.relativePath) &&
                        PathUtils::ToLower(other.relativePath.generic_string()).find(needle) != std::string::npos)
                    {
                        descendantMatches = true;
                        break;
                    }
                }
            }

            if (filtering)
            {
                if (!selfMatches && !descendantMatches)
                {
                    continue;
                }
            }
            else if (HasCollapsedAncestor(entry.relativePath, collapsedFolders))
            {
                continue;
            }

            TreeViewRow row;
            row.relativePath = entry.relativePath;
            row.isDirectory = entry.isDirectory;
            row.depth = entry.depth;
            row.selectable = mode == TreeViewMode::Folders ? entry.isDirectory : true;
            row.matchedFilter = selfMatches && filtering;
            rows.push_back(std::move(row));
        }

        return rows;
    }

    std::string TreePathKey(const fs::path& relativePath)
    {
        return PathUtils::ToLower(PathUtils::NormalizeRelative(relativePath).generic_string());
    }

    bool PathIsDescendantOf(const fs::path& candidate, const fs::path& folder)
    {
        const fs::path normalizedCandidate = PathUtils::NormalizeRelative(candidate);
        const fs::path normalizedFolder = PathUtils::NormalizeRelative(folder);
        auto folderIt = normalizedFolder.begin();
        auto candidateIt = normalizedCandidate.begin();
        for (; folderIt != normalizedFolder.end(); ++folderIt, ++candidateIt)
        {
            if (candidateIt == normalizedCandidate.end() || *folderIt != *candidateIt)
            {
                return false;
            }
        }
        return candidateIt != normalizedCandidate.end();
    }
}
