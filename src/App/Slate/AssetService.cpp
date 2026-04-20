#include "App/Slate/AssetService.h"

#include "App/Slate/PathUtils.h"

#include <fstream>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace Software::Slate
{
    AssetService::AssetService(fs::path workspaceRoot)
        : m_workspaceRoot(std::move(workspaceRoot))
    {
    }

    void AssetService::SetWorkspaceRoot(fs::path workspaceRoot)
    {
        m_workspaceRoot = std::move(workspaceRoot);
    }

    fs::path AssetService::AssetRelativePathFor(const fs::path& noteRelativePath, const fs::path& sourceImagePath) const
    {
        const std::string extension = sourceImagePath.extension().empty() ? ".png" : sourceImagePath.extension().string();
        return AssetRelativePathForExtension(noteRelativePath, sourceImagePath.stem().string(), extension);
    }

    bool AssetService::CopyImageAsset(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                      fs::path* assetRelativePath, std::string* error) const
    {
        if (!PathUtils::IsImageFile(sourceImagePath))
        {
            if (error)
            {
                *error = "Dropped file is not a supported image.";
            }
            return false;
        }

        const fs::path relative = AssetRelativePathFor(noteRelativePath, sourceImagePath);
        const fs::path absolute = m_workspaceRoot / relative;
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

        fs::copy_file(sourceImagePath, absolute, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        if (assetRelativePath)
        {
            *assetRelativePath = relative;
        }
        return true;
    }

    bool AssetService::HasClipboardImage() const
    {
#if defined(_WIN32)
        return IsClipboardFormatAvailable(CF_DIB) != 0;
#else
        return false;
#endif
    }

    bool AssetService::SaveClipboardImageAsset(const fs::path& noteRelativePath, fs::path* assetRelativePath,
                                               std::string* error) const
    {
#if defined(_WIN32)
        if (!HasClipboardImage())
        {
            if (error)
            {
                *error = "Clipboard does not contain an image.";
            }
            return false;
        }

        if (!OpenClipboard(nullptr))
        {
            if (error)
            {
                *error = "Could not open clipboard.";
            }
            return false;
        }

        HANDLE handle = GetClipboardData(CF_DIB);
        if (!handle)
        {
            CloseClipboard();
            if (error)
            {
                *error = "Could not read clipboard image.";
            }
            return false;
        }

        const auto* dib = static_cast<const unsigned char*>(GlobalLock(handle));
        const SIZE_T dibSize = GlobalSize(handle);
        if (!dib || dibSize < sizeof(BITMAPINFOHEADER))
        {
            if (dib)
            {
                GlobalUnlock(handle);
            }
            CloseClipboard();
            if (error)
            {
                *error = "Clipboard image is not a DIB bitmap.";
            }
            return false;
        }

        const auto* header = reinterpret_cast<const BITMAPINFOHEADER*>(dib);
        DWORD colorCount = header->biClrUsed;
        if (colorCount == 0 && header->biBitCount <= 8)
        {
            colorCount = 1u << header->biBitCount;
        }

        DWORD maskBytes = 0;
        if (header->biCompression == BI_BITFIELDS && header->biSize == sizeof(BITMAPINFOHEADER))
        {
            maskBytes = 3u * sizeof(DWORD);
        }

        const DWORD pixelOffset = header->biSize + maskBytes + colorCount * sizeof(RGBQUAD);
        BITMAPFILEHEADER fileHeader{};
        fileHeader.bfType = 0x4D42;
        fileHeader.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + pixelOffset);
        fileHeader.bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + dibSize);

        const fs::path relative = AssetRelativePathForExtension(noteRelativePath, "clipboard", ".bmp");
        const fs::path absolute = m_workspaceRoot / relative;
        std::error_code ec;
        fs::create_directories(absolute.parent_path(), ec);
        if (ec)
        {
            GlobalUnlock(handle);
            CloseClipboard();
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        std::ofstream file(absolute, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
        file.write(reinterpret_cast<const char*>(dib), static_cast<std::streamsize>(dibSize));
        const bool ok = static_cast<bool>(file);

        GlobalUnlock(handle);
        CloseClipboard();

        if (!ok)
        {
            if (error)
            {
                *error = "Could not write pasted image.";
            }
            return false;
        }

        if (assetRelativePath)
        {
            *assetRelativePath = relative;
        }
        return true;
#else
        (void)noteRelativePath;
        (void)assetRelativePath;
        if (error)
        {
            *error = "Clipboard image paste is only implemented on Windows.";
        }
        return false;
#endif
    }

    std::string AssetService::MarkdownImageLink(const fs::path& noteRelativePath, const fs::path& assetRelativePath)
    {
        std::error_code ec;
        fs::path relative = fs::relative(assetRelativePath, noteRelativePath.parent_path(), ec);
        if (ec)
        {
            relative = assetRelativePath;
        }
        return "![](" + relative.generic_string() + ")";
    }

    fs::path AssetService::AssetRelativePathForExtension(const fs::path& noteRelativePath, const std::string& stem,
                                                         const std::string& extension) const
    {
        const std::string noteName = PathUtils::SanitizeFileName(noteRelativePath.stem().string());
        const fs::path folder = fs::path("assets") / noteName;
        const std::string baseName = PathUtils::CurrentTimestampForFile() + "_" + PathUtils::SanitizeFileName(stem);
        fs::path candidate = folder / (baseName + extension);
        for (int i = 2; fs::exists(m_workspaceRoot / candidate) && i < 1000; ++i)
        {
            candidate = folder / (baseName + "_" + std::to_string(i) + extension);
        }
        return candidate;
    }
}
