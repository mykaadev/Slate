#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>

namespace Software::Slate
{
    class AssetService
    {
    public:
        explicit AssetService(fs::path workspaceRoot = {});

        void SetWorkspaceRoot(fs::path workspaceRoot);
        fs::path AssetRelativePathFor(const fs::path& noteRelativePath, const fs::path& sourceImagePath) const;
        fs::path AssetRelativePathFor(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                      ImageStoragePolicy policy) const;
        bool CopyImageAsset(const fs::path& noteRelativePath, const fs::path& sourceImagePath, fs::path* assetRelativePath,
                            std::string* error = nullptr) const;
        bool CopyImageAsset(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                            ImageStoragePolicy policy, fs::path* assetRelativePath, std::string* error = nullptr) const;
        bool CreateMarkdownImageFromFile(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                         ImageStoragePolicy policy, std::string* markdown,
                                         std::string* error = nullptr) const;
        bool HasClipboardImage() const;
        bool SaveClipboardImageAsset(const fs::path& noteRelativePath, fs::path* assetRelativePath,
                                     std::string* error = nullptr) const;
        bool SaveClipboardImageAsset(const fs::path& noteRelativePath, ImageStoragePolicy policy,
                                     fs::path* assetRelativePath, std::string* error = nullptr) const;
        bool CreateMarkdownImageFromClipboard(const fs::path& noteRelativePath, ImageStoragePolicy policy,
                                              std::string* markdown, std::string* error = nullptr) const;
        static std::string MarkdownImageLink(const fs::path& noteRelativePath, const fs::path& assetRelativePath);
        static std::string MarkdownImageTarget(const std::string& target);

    private:
        fs::path AssetRelativePathForExtension(const fs::path& noteRelativePath, const std::string& stem,
                                               const std::string& extension) const;
        fs::path AssetRelativePathForExtension(const fs::path& noteRelativePath, const std::string& stem,
                                               const std::string& extension, ImageStoragePolicy policy) const;

        fs::path m_workspaceRoot;
    };
}
