#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>

namespace Software::Slate
{
    class HistoryService
    {
    public:
        explicit HistoryService(fs::path workspaceRoot = {});

        void SetWorkspaceRoot(fs::path workspaceRoot);
        bool SnapshotFile(const fs::path& absolutePath, const fs::path& relativePath, std::string* error = nullptr) const;
        bool SnapshotText(const fs::path& relativePath, const std::string& text, std::string* error = nullptr) const;

    private:
        fs::path HistoryDir() const;
        fs::path SnapshotPathFor(const fs::path& relativePath) const;

        fs::path m_workspaceRoot;
    };
}
