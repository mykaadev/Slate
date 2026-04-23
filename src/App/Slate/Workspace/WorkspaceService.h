#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    // Scans and mutates files inside one workspace root
    class WorkspaceService
    {
    public:
        // Builds the service with an optional root
        explicit WorkspaceService(fs::path root = {});

        // Opens a workspace root and scans it
        bool Open(fs::path root, std::string* error = nullptr);
        // Rescans the workspace tree
        bool Scan(std::string* error = nullptr);

        // Returns the workspace root
        const fs::path& Root() const;
        // Returns the scanned workspace entries
        const std::vector<WorkspaceEntry>& Entries() const;
        // Returns the markdown files in scan order
        const std::vector<fs::path>& MarkdownFiles() const;
        // Returns the recent file list
        const std::vector<fs::path>& RecentFiles() const;

        // Resolves a relative path inside the workspace
        fs::path Resolve(const fs::path& relativePath) const;
        // Converts an absolute path to a workspace relative path
        fs::path MakeRelative(const fs::path& absolutePath) const;

        // Creates a markdown file with initial text
        bool CreateMarkdownFile(const fs::path& relativePath, const std::string& initialText, fs::path* createdPath,
                                std::string* error = nullptr);
        // Creates a folder inside the workspace
        bool CreateFolder(const fs::path& folderRelativePath, const std::string& requestedName, fs::path* createdPath,
                          std::string* error = nullptr);
        // Renames or moves one path
        bool RenameOrMove(const fs::path& oldRelativePath, const fs::path& newRelativePath, std::string* error = nullptr);
        // Deletes one path from the workspace
        bool DeletePath(const fs::path& relativePath, std::string* error = nullptr);
        // Builds a collision safe markdown path
        fs::path MakeCollisionSafeMarkdownPath(const fs::path& folderRelativePath, const std::string& requestedName) const;
        // Builds a collision safe folder path
        fs::path MakeCollisionSafeFolderPath(const fs::path& folderRelativePath, const std::string& requestedName) const;

        // Returns the current daily note path
        fs::path DailyNoteRelativePath() const;
        // Creates the daily note if needed
        bool EnsureDailyNote(fs::path* relativePath, std::string* error = nullptr);

        // Moves one file to the front of recents
        void TouchRecent(const fs::path& relativePath);
        // Loads recents from disk
        void LoadRecent();
        // Saves recents to disk
        void SaveRecent() const;

    private:
        // Filters names that should not appear in the tree
        static bool ShouldIgnoreName(const fs::path& path);
        // Validates that a path stays inside the workspace
        bool EnsureInsideWorkspace(const fs::path& path, std::string* error) const;
        // Returns the internal slate metadata folder
        fs::path SlateDir() const;

        // Stores the workspace root
        fs::path m_root;
        // Stores the full scanned tree
        std::vector<WorkspaceEntry> m_entries;
        // Stores scanned markdown files
        std::vector<fs::path> m_markdownFiles;
        // Stores recently opened files
        std::vector<fs::path> m_recentFiles;
    };
}
