#pragma once

#include "App/Slate/SlateTypes.h"

#include <string>

namespace Software::Slate
{
    class AssetService
    {
    public:
        explicit AssetService(fs::path workspaceRoot = {});

        void SetWorkspaceRoot(fs::path workspaceRoot);
        fs::path AssetRelativePathFor(const fs::path& noteRelativePath, const fs::path& sourceImagePath) const;
        bool CopyImageAsset(const fs::path& noteRelativePath, const fs::path& sourceImagePath, fs::path* assetRelativePath,
                            std::string* error = nullptr) const;
        bool HasClipboardImage() const;
        bool SaveClipboardImageAsset(const fs::path& noteRelativePath, fs::path* assetRelativePath,
                                     std::string* error = nullptr) const;
        static std::string MarkdownImageLink(const fs::path& noteRelativePath, const fs::path& assetRelativePath);

    private:
        fs::path AssetRelativePathForExtension(const fs::path& noteRelativePath, const std::string& stem,
                                               const std::string& extension) const;

        fs::path m_workspaceRoot;
    };
}
