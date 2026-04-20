#include "App/Slate/WorkspaceService.h"

#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Software::Slate
{
    WorkspaceService::WorkspaceService(fs::path root)
    {
        if (root.empty())
        {
            return;
        }
        std::string ignored;
        Open(std::move(root), &ignored);
    }

    bool WorkspaceService::Open(fs::path root, std::string* error)
    {
        std::error_code ec;
        root = fs::weakly_canonical(root, ec);
        if (ec || !fs::exists(root) || !fs::is_directory(root))
        {
            if (error)
            {
                *error = "Workspace path is not a folder.";
            }
            return false;
        }

        m_root = std::move(root);
        fs::create_directories(SlateDir(), ec);
        LoadRecent();
        return Scan(error);
    }

    bool WorkspaceService::Scan(std::string* error)
    {
        m_entries.clear();
        m_markdownFiles.clear();

        std::error_code ec;
        if (!fs::exists(m_root, ec))
        {
            if (error)
            {
                *error = "Workspace no longer exists.";
            }
            return false;
        }

        fs::recursive_directory_iterator it(m_root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        while (!ec && it != end)
        {
            const fs::path path = it->path();
            const bool isDirectory = it->is_directory(ec);
            if (ShouldIgnoreName(path))
            {
                if (isDirectory)
                {
                    it.disable_recursion_pending();
                }
                it.increment(ec);
                continue;
            }

            WorkspaceEntry entry;
            entry.path = path;
            entry.relativePath = MakeRelative(path);
            entry.isDirectory = isDirectory;
            entry.depth = it.depth();
            m_entries.push_back(entry);

            if (!isDirectory && PathUtils::IsMarkdownFile(path))
            {
                m_markdownFiles.push_back(entry.relativePath);
            }

            it.increment(ec);
        }

        std::sort(m_entries.begin(), m_entries.end(), [](const WorkspaceEntry& a, const WorkspaceEntry& b) {
            if (a.relativePath.parent_path() == b.relativePath.parent_path() && a.isDirectory != b.isDirectory)
            {
                return a.isDirectory;
            }
            return PathUtils::ToLower(a.relativePath.generic_string()) <
                   PathUtils::ToLower(b.relativePath.generic_string());
        });

        std::sort(m_markdownFiles.begin(), m_markdownFiles.end(), [](const fs::path& a, const fs::path& b) {
            return PathUtils::ToLower(a.generic_string()) < PathUtils::ToLower(b.generic_string());
        });

        if (ec && error)
        {
            *error = ec.message();
        }
        return !ec;
    }

    const fs::path& WorkspaceService::Root() const
    {
        return m_root;
    }

    const std::vector<WorkspaceEntry>& WorkspaceService::Entries() const
    {
        return m_entries;
    }

    const std::vector<fs::path>& WorkspaceService::MarkdownFiles() const
    {
        return m_markdownFiles;
    }

    const std::vector<fs::path>& WorkspaceService::RecentFiles() const
    {
        return m_recentFiles;
    }

    fs::path WorkspaceService::Resolve(const fs::path& relativePath) const
    {
        return (m_root / relativePath).lexically_normal();
    }

    fs::path WorkspaceService::MakeRelative(const fs::path& absolutePath) const
    {
        std::error_code ec;
        fs::path relative = fs::relative(absolutePath, m_root, ec);
        if (ec)
        {
            return absolutePath.filename();
        }
        return PathUtils::NormalizeRelative(relative);
    }

    bool WorkspaceService::CreateMarkdownFile(const fs::path& relativePath, const std::string& initialText,
                                              fs::path* createdPath, std::string* error)
    {
        fs::path target = relativePath;
        if (!PathUtils::IsMarkdownFile(target))
        {
            target += ".md";
        }
        target = PathUtils::NormalizeRelative(target);

        const fs::path absolute = Resolve(target);
        if (!EnsureInsideWorkspace(absolute, error))
        {
            return false;
        }

        std::error_code ec;
        fs::create_directories(absolute.parent_path(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        if (fs::exists(absolute, ec))
        {
            if (error)
            {
                *error = "File already exists.";
            }
            return false;
        }

        std::ofstream file(absolute, std::ios::binary);
        if (!file)
        {
            if (error)
            {
                *error = "Could not create file.";
            }
            return false;
        }
        file << initialText;
        file.close();

        if (createdPath)
        {
            *createdPath = target;
        }
        TouchRecent(target);
        Scan(nullptr);
        return true;
    }

    bool WorkspaceService::RenameOrMove(const fs::path& oldRelativePath, const fs::path& newRelativePath,
                                        std::string* error)
    {
        const fs::path oldAbsolute = Resolve(PathUtils::NormalizeRelative(oldRelativePath));
        fs::path newRelative = newRelativePath;
        if (PathUtils::IsMarkdownFile(oldAbsolute) && !PathUtils::IsMarkdownFile(newRelative))
        {
            newRelative += ".md";
        }
        const fs::path newAbsolute = Resolve(PathUtils::NormalizeRelative(newRelative));

        if (!EnsureInsideWorkspace(oldAbsolute, error) || !EnsureInsideWorkspace(newAbsolute, error))
        {
            return false;
        }

        std::error_code ec;
        if (!fs::exists(oldAbsolute, ec))
        {
            if (error)
            {
                *error = "Source path does not exist.";
            }
            return false;
        }
        if (fs::exists(newAbsolute, ec))
        {
            if (error)
            {
                *error = "Target path already exists.";
            }
            return false;
        }

        fs::create_directories(newAbsolute.parent_path(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        fs::rename(oldAbsolute, newAbsolute, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        for (auto& recent : m_recentFiles)
        {
            if (recent == oldRelativePath)
            {
                recent = PathUtils::NormalizeRelative(newRelative);
            }
        }
        SaveRecent();
        Scan(nullptr);
        return true;
    }

    bool WorkspaceService::DeletePath(const fs::path& relativePath, std::string* error)
    {
        const fs::path absolute = Resolve(PathUtils::NormalizeRelative(relativePath));
        if (!EnsureInsideWorkspace(absolute, error))
        {
            return false;
        }

        std::error_code ec;
        if (!fs::exists(absolute, ec))
        {
            if (error)
            {
                *error = "Path does not exist.";
            }
            return false;
        }

        fs::remove_all(absolute, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), relativePath), m_recentFiles.end());
        SaveRecent();
        Scan(nullptr);
        return true;
    }

    fs::path WorkspaceService::MakeCollisionSafeMarkdownPath(const fs::path& folderRelativePath,
                                                             const std::string& requestedName) const
    {
        fs::path folder = PathUtils::NormalizeRelative(folderRelativePath);
        if (folder == ".")
        {
            folder.clear();
        }

        std::string trimmed = PathUtils::Trim(requestedName);
        if (trimmed.empty())
        {
            trimmed = "Untitled";
        }

        fs::path namePath(trimmed);
        std::string stem = PathUtils::SanitizeFileName(namePath.stem().string());
        if (stem.empty())
        {
            stem = "Untitled";
        }

        std::string extension = namePath.extension().string();
        if (extension.empty())
        {
            extension = ".md";
        }
        else if (!PathUtils::IsMarkdownFile(namePath))
        {
            stem = PathUtils::SanitizeFileName(namePath.filename().string());
            extension = ".md";
        }

        fs::path candidate = PathUtils::NormalizeRelative(folder / (stem + extension));
        for (int suffix = 2; fs::exists(Resolve(candidate)) && suffix < 10000; ++suffix)
        {
            candidate = PathUtils::NormalizeRelative(folder / (stem + "-" + std::to_string(suffix) + extension));
        }
        return candidate;
    }

    fs::path WorkspaceService::DailyNoteRelativePath() const
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#if defined(_WIN32)
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif

        std::ostringstream path;
        path << "Journal/" << std::put_time(&localTime, "%Y/%m/%Y-%m-%d") << ".md";
        return fs::path(path.str());
    }

    bool WorkspaceService::EnsureDailyNote(fs::path* relativePath, std::string* error)
    {
        const fs::path daily = DailyNoteRelativePath();
        const fs::path absolute = Resolve(daily);

        std::error_code ec;
        if (!fs::exists(absolute, ec))
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm localTime{};
#if defined(_WIN32)
            localtime_s(&localTime, &time);
#else
            localtime_r(&time, &localTime);
#endif

            std::ostringstream text;
            text << "# " << std::put_time(&localTime, "%Y-%m-%d") << "\n\n- ";
            fs::path created;
            if (!CreateMarkdownFile(daily, text.str(), &created, error))
            {
                return false;
            }
        }

        if (relativePath)
        {
            *relativePath = daily;
        }
        TouchRecent(daily);
        return true;
    }

    void WorkspaceService::TouchRecent(const fs::path& relativePath)
    {
        const fs::path normalized = PathUtils::NormalizeRelative(relativePath);
        m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), normalized), m_recentFiles.end());
        m_recentFiles.insert(m_recentFiles.begin(), normalized);
        if (m_recentFiles.size() > 20)
        {
            m_recentFiles.resize(20);
        }
        SaveRecent();
    }

    void WorkspaceService::LoadRecent()
    {
        m_recentFiles.clear();
        std::ifstream file(SlateDir() / "recent.txt", std::ios::binary);
        std::string line;
        while (std::getline(file, line))
        {
            line = PathUtils::Trim(line);
            if (!line.empty())
            {
                m_recentFiles.emplace_back(line);
            }
        }
    }

    void WorkspaceService::SaveRecent() const
    {
        std::error_code ec;
        fs::create_directories(SlateDir(), ec);
        std::ofstream file(SlateDir() / "recent.txt", std::ios::binary | std::ios::trunc);
        for (const auto& recent : m_recentFiles)
        {
            file << recent.generic_string() << '\n';
        }
    }

    bool WorkspaceService::ShouldIgnoreName(const fs::path& path)
    {
        const std::string name = PathUtils::ToLower(path.filename().string());
        if (name.empty())
        {
            return false;
        }

        return name == ".git" || name == ".slate" || name == ".vs" || name == ".idea" || name == "build" ||
               name == "bin" || name == "out" || name == "_deps" || name == "cmakefiles" ||
               name.rfind("cmake-build-", 0) == 0 || name == ".ds_store" || name == "thumbs.db";
    }

    bool WorkspaceService::EnsureInsideWorkspace(const fs::path& path, std::string* error) const
    {
        if (!PathUtils::IsSubPath(m_root, path))
        {
            if (error)
            {
                *error = "Path escapes the workspace.";
            }
            return false;
        }
        return true;
    }

    fs::path WorkspaceService::SlateDir() const
    {
        return m_root / ".slate";
    }
}
