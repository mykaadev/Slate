#include "App/Slate/Documents/AssetService.h"

#include "App/Slate/Core/PathUtils.h"

#include <fstream>
#include <vector>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace Software::Slate
{
    namespace
    {
#if defined(_WIN32)
        bool ClipboardHasImageFormat()
        {
            return IsClipboardFormatAvailable(CF_DIBV5) != 0 ||
                   IsClipboardFormatAvailable(CF_DIB) != 0 ||
                   IsClipboardFormatAvailable(CF_BITMAP) != 0;
        }

        HANDLE FirstClipboardDibHandle()
        {
            if (IsClipboardFormatAvailable(CF_DIBV5) != 0)
            {
                return GetClipboardData(CF_DIBV5);
            }
            if (IsClipboardFormatAvailable(CF_DIB) != 0)
            {
                return GetClipboardData(CF_DIB);
            }
            return nullptr;
        }

        bool WriteBitmapFileFromDibHandle(HANDLE handle, const fs::path& absolutePath, std::string* error)
        {
            if (!handle)
            {
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

            std::ofstream file(absolutePath, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
            file.write(reinterpret_cast<const char*>(dib), static_cast<std::streamsize>(dibSize));
            const bool ok = static_cast<bool>(file);
            GlobalUnlock(handle);

            if (!ok && error)
            {
                *error = "Could not write pasted image.";
            }
            return ok;
        }

        bool WriteBitmapFileFromBitmapHandle(HBITMAP bitmapHandle, const fs::path& absolutePath, std::string* error)
        {
            if (!bitmapHandle)
            {
                if (error)
                {
                    *error = "Could not read clipboard image.";
                }
                return false;
            }

            BITMAP bitmap{};
            if (GetObjectW(bitmapHandle, sizeof(bitmap), &bitmap) == 0)
            {
                if (error)
                {
                    *error = "Could not inspect clipboard bitmap.";
                }
                return false;
            }

            BITMAPINFOHEADER header{};
            header.biSize = sizeof(BITMAPINFOHEADER);
            header.biWidth = bitmap.bmWidth;
            header.biHeight = bitmap.bmHeight;
            header.biPlanes = 1;
            header.biBitCount = 32;
            header.biCompression = BI_RGB;

            const DWORD imageSize = static_cast<DWORD>(bitmap.bmWidth * bitmap.bmHeight * 4);
            std::vector<unsigned char> pixels(imageSize);

            BITMAPINFO info{};
            info.bmiHeader = header;
            HDC deviceContext = GetDC(nullptr);
            const int copied = GetDIBits(deviceContext,
                                         bitmapHandle,
                                         0,
                                         static_cast<UINT>(bitmap.bmHeight),
                                         pixels.data(),
                                         &info,
                                         DIB_RGB_COLORS);
            ReleaseDC(nullptr, deviceContext);
            if (copied == 0)
            {
                if (error)
                {
                    *error = "Could not copy clipboard bitmap.";
                }
                return false;
            }

            BITMAPFILEHEADER fileHeader{};
            fileHeader.bfType = 0x4D42;
            fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            fileHeader.bfSize = fileHeader.bfOffBits + imageSize;

            std::ofstream file(absolutePath, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
            file.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
            file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
            const bool ok = static_cast<bool>(file);
            if (!ok && error)
            {
                *error = "Could not write pasted image.";
            }
            return ok;
        }
#endif
    }

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
        return ClipboardHasImageFormat();
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

        const fs::path relative = AssetRelativePathForExtension(noteRelativePath, "clipboard", ".bmp");
        const fs::path absolute = m_workspaceRoot / relative;
        std::error_code ec;
        fs::create_directories(absolute.parent_path(), ec);
        if (ec)
        {
            CloseClipboard();
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        const HANDLE dibHandle = FirstClipboardDibHandle();
        bool ok = false;
        if (dibHandle)
        {
            ok = WriteBitmapFileFromDibHandle(dibHandle, absolute, error);
        }
        else
        {
            ok = WriteBitmapFileFromBitmapHandle(static_cast<HBITMAP>(GetClipboardData(CF_BITMAP)), absolute, error);
        }
        CloseClipboard();

        if (!ok)
        {
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
