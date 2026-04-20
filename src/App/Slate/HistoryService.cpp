#include "App/Slate/HistoryService.h"

#include "App/Slate/PathUtils.h"

#include <fstream>
#include <utility>

namespace Software::Slate
{
    HistoryService::HistoryService(fs::path workspaceRoot)
        : m_workspaceRoot(std::move(workspaceRoot))
    {
    }

    void HistoryService::SetWorkspaceRoot(fs::path workspaceRoot)
    {
        m_workspaceRoot = std::move(workspaceRoot);
    }

    bool HistoryService::SnapshotFile(const fs::path& absolutePath, const fs::path& relativePath, std::string* error) const
    {
        std::error_code ec;
        if (!fs::exists(absolutePath, ec) || fs::is_directory(absolutePath, ec))
        {
            return true;
        }

        fs::create_directories(HistoryDir(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        fs::copy_file(absolutePath, SnapshotPathFor(relativePath), fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }
        return true;
    }

    bool HistoryService::SnapshotText(const fs::path& relativePath, const std::string& text, std::string* error) const
    {
        std::error_code ec;
        fs::create_directories(HistoryDir(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        std::ofstream file(SnapshotPathFor(relativePath), std::ios::binary | std::ios::trunc);
        if (!file)
        {
            if (error)
            {
                *error = "Could not write history snapshot.";
            }
            return false;
        }
        file << text;
        return true;
    }

    fs::path HistoryService::HistoryDir() const
    {
        return m_workspaceRoot / ".slate" / "history";
    }

    fs::path HistoryService::SnapshotPathFor(const fs::path& relativePath) const
    {
        const std::string id = PathUtils::StablePathId(relativePath);
        const std::string safeName = PathUtils::SanitizeFileName(relativePath.filename().string());
        return HistoryDir() / (PathUtils::CurrentTimestampForFile() + "_" + id + "_" + safeName);
    }
}
