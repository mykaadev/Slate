#pragma once

#include "App/Slate/SlateTypes.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    class WorkspaceService
    {
    public:
        explicit WorkspaceService(fs::path root = {});

        bool Open(fs::path root, std::string* error = nullptr);
        bool Scan(std::string* error = nullptr);

        const fs::path& Root() const;
        const std::vector<WorkspaceEntry>& Entries() const;
        const std::vector<fs::path>& MarkdownFiles() const;
        const std::vector<fs::path>& RecentFiles() const;

        fs::path Resolve(const fs::path& relativePath) const;
        fs::path MakeRelative(const fs::path& absolutePath) const;

        bool CreateMarkdownFile(const fs::path& relativePath, const std::string& initialText, fs::path* createdPath,
                                std::string* error = nullptr);
        bool RenameOrMove(const fs::path& oldRelativePath, const fs::path& newRelativePath, std::string* error = nullptr);
        bool DeletePath(const fs::path& relativePath, std::string* error = nullptr);
        fs::path MakeCollisionSafeMarkdownPath(const fs::path& folderRelativePath, const std::string& requestedName) const;

        fs::path DailyNoteRelativePath() const;
        bool EnsureDailyNote(fs::path* relativePath, std::string* error = nullptr);

        void TouchRecent(const fs::path& relativePath);
        void LoadRecent();
        void SaveRecent() const;

    private:
        static bool ShouldIgnoreName(const fs::path& path);
        bool EnsureInsideWorkspace(const fs::path& path, std::string* error) const;
        fs::path SlateDir() const;

        fs::path m_root;
        std::vector<WorkspaceEntry> m_entries;
        std::vector<fs::path> m_markdownFiles;
        std::vector<fs::path> m_recentFiles;
    };
}
