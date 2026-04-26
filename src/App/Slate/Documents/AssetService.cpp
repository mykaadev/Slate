#include "App/Slate/Documents/AssetService.h"

#include "App/Slate/Core/PathUtils.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
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
        std::string MimeTypeForExtension(const std::string& extension)
        {
            const std::string ext = PathUtils::ToLower(extension);
            if (ext == ".jpg" || ext == ".jpeg")
            {
                return "image/jpeg";
            }
            if (ext == ".gif")
            {
                return "image/gif";
            }
            if (ext == ".bmp")
            {
                return "image/bmp";
            }
            if (ext == ".webp")
            {
                return "image/webp";
            }
            return "image/png";
        }

        std::string Base64Encode(const std::vector<unsigned char>& bytes)
        {
            static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            out.reserve(((bytes.size() + 2) / 3) * 4);

            std::size_t i = 0;
            while (i + 3 <= bytes.size())
            {
                const unsigned value = (static_cast<unsigned>(bytes[i]) << 16) |
                                       (static_cast<unsigned>(bytes[i + 1]) << 8) |
                                       static_cast<unsigned>(bytes[i + 2]);
                out.push_back(table[(value >> 18) & 0x3F]);
                out.push_back(table[(value >> 12) & 0x3F]);
                out.push_back(table[(value >> 6) & 0x3F]);
                out.push_back(table[value & 0x3F]);
                i += 3;
            }

            if (i < bytes.size())
            {
                unsigned value = static_cast<unsigned>(bytes[i]) << 16;
                out.push_back(table[(value >> 18) & 0x3F]);
                if (i + 1 < bytes.size())
                {
                    value |= static_cast<unsigned>(bytes[i + 1]) << 8;
                    out.push_back(table[(value >> 12) & 0x3F]);
                    out.push_back(table[(value >> 6) & 0x3F]);
                    out.push_back('=');
                }
                else
                {
                    out.push_back(table[(value >> 12) & 0x3F]);
                    out.push_back('=');
                    out.push_back('=');
                }
            }

            return out;
        }

        bool ReadFileBytes(const fs::path& path, std::vector<unsigned char>* out, std::string* error)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                if (error)
                {
                    *error = "Could not read image file.";
                }
                return false;
            }

            std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (file.bad())
            {
                if (error)
                {
                    *error = "Could not read image bytes.";
                }
                return false;
            }

            if (out)
            {
                *out = std::move(bytes);
            }
            return true;
        }

        bool WriteBytes(const fs::path& path, const std::vector<unsigned char>& bytes, std::string* error)
        {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            if (ec)
            {
                if (error)
                {
                    *error = ec.message();
                }
                return false;
            }

            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!file)
            {
                if (error)
                {
                    *error = "Could not write image file.";
                }
                return false;
            }
            return true;
        }

        std::string DataUriForBytes(const std::vector<unsigned char>& bytes, const std::string& extension)
        {
            return "data:" + MimeTypeForExtension(extension) + ";base64," + Base64Encode(bytes);
        }

        bool IsMarkdownUnsafeBareTarget(std::string_view target)
        {
            for (const char ch : target)
            {
                if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')')
                {
                    return true;
                }
            }
            return false;
        }

        std::string EscapeAngleTarget(std::string target)
        {
            for (char& ch : target)
            {
                if (ch == '\\')
                {
                    ch = '/';
                }
            }
            return target;
        }

        std::string AbsolutePathTarget(const fs::path& path)
        {
            return EscapeAngleTarget(path.lexically_normal().generic_string());
        }

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

        bool BitmapFileBytesFromDibHandle(HANDLE handle, std::vector<unsigned char>* out, std::string* error)
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

            std::vector<unsigned char> bytes(sizeof(BITMAPFILEHEADER) + static_cast<std::size_t>(dibSize));
            std::memcpy(bytes.data(), &fileHeader, sizeof(fileHeader));
            std::memcpy(bytes.data() + sizeof(BITMAPFILEHEADER), dib, static_cast<std::size_t>(dibSize));
            GlobalUnlock(handle);

            if (out)
            {
                *out = std::move(bytes);
            }
            return true;
        }

        bool BitmapFileBytesFromBitmapHandle(HBITMAP bitmapHandle, std::vector<unsigned char>* out, std::string* error)
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

            std::vector<unsigned char> bytes(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixels.size());
            unsigned char* cursor = bytes.data();
            std::memcpy(cursor, &fileHeader, sizeof(fileHeader));
            cursor += sizeof(fileHeader);
            std::memcpy(cursor, &info.bmiHeader, sizeof(info.bmiHeader));
            cursor += sizeof(info.bmiHeader);
            std::memcpy(cursor, pixels.data(), pixels.size());

            if (out)
            {
                *out = std::move(bytes);
            }
            return true;
        }

        bool ClipboardBitmapBytes(std::vector<unsigned char>* out, std::string* error)
        {
            const HANDLE dibHandle = FirstClipboardDibHandle();
            if (dibHandle)
            {
                return BitmapFileBytesFromDibHandle(dibHandle, out, error);
            }
            return BitmapFileBytesFromBitmapHandle(static_cast<HBITMAP>(GetClipboardData(CF_BITMAP)), out, error);
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
        return AssetRelativePathFor(noteRelativePath, sourceImagePath, ImageStoragePolicy::WorkspaceMediaFolder);
    }

    fs::path AssetService::AssetRelativePathFor(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                                ImageStoragePolicy policy) const
    {
        const std::string extension = sourceImagePath.extension().empty() ? ".png" : sourceImagePath.extension().string();
        return AssetRelativePathForExtension(noteRelativePath, sourceImagePath.stem().string(), extension, policy);
    }

    bool AssetService::CopyImageAsset(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                      fs::path* assetRelativePath, std::string* error) const
    {
        return CopyImageAsset(noteRelativePath, sourceImagePath, ImageStoragePolicy::WorkspaceMediaFolder,
                              assetRelativePath, error);
    }

    bool AssetService::CopyImageAsset(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                      ImageStoragePolicy policy, fs::path* assetRelativePath, std::string* error) const
    {
        if (!PathUtils::IsImageFile(sourceImagePath))
        {
            if (error)
            {
                *error = "Dropped file is not a supported image.";
            }
            return false;
        }

        if (policy == ImageStoragePolicy::LinkOriginal || policy == ImageStoragePolicy::EmbedInMarkdown)
        {
            if (error)
            {
                *error = "This image storage policy does not copy files.";
            }
            return false;
        }

        const fs::path relative = AssetRelativePathFor(noteRelativePath, sourceImagePath, policy);
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

    bool AssetService::CreateMarkdownImageFromFile(const fs::path& noteRelativePath, const fs::path& sourceImagePath,
                                                   ImageStoragePolicy policy, std::string* markdown,
                                                   std::string* error) const
    {
        if (!PathUtils::IsImageFile(sourceImagePath))
        {
            if (error)
            {
                *error = "Dropped file is not a supported image.";
            }
            return false;
        }

        if (policy == ImageStoragePolicy::EmbedInMarkdown)
        {
            std::vector<unsigned char> bytes;
            if (!ReadFileBytes(sourceImagePath, &bytes, error))
            {
                return false;
            }
            if (markdown)
            {
                *markdown = MarkdownImageTarget(DataUriForBytes(bytes, sourceImagePath.extension().string()));
            }
            return true;
        }

        if (policy == ImageStoragePolicy::LinkOriginal)
        {
            fs::path target;
            std::error_code ec;
            const fs::path absoluteSource = fs::absolute(sourceImagePath, ec).lexically_normal();
            if (!ec && !m_workspaceRoot.empty() && PathUtils::IsSubPath(m_workspaceRoot, absoluteSource))
            {
                const fs::path workspaceRelative = fs::relative(absoluteSource, m_workspaceRoot, ec);
                if (!ec)
                {
                    if (markdown)
                    {
                        *markdown = MarkdownImageLink(noteRelativePath, workspaceRelative);
                    }
                    return true;
                }
            }

            if (markdown)
            {
                *markdown = MarkdownImageTarget(AbsolutePathTarget(absoluteSource.empty() ? sourceImagePath : absoluteSource));
            }
            return true;
        }

        fs::path assetRelative;
        if (!CopyImageAsset(noteRelativePath, sourceImagePath, policy, &assetRelative, error))
        {
            return false;
        }
        if (markdown)
        {
            *markdown = MarkdownImageLink(noteRelativePath, assetRelative);
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
        return SaveClipboardImageAsset(noteRelativePath, ImageStoragePolicy::WorkspaceMediaFolder, assetRelativePath,
                                       error);
    }

    bool AssetService::SaveClipboardImageAsset(const fs::path& noteRelativePath, ImageStoragePolicy policy,
                                               fs::path* assetRelativePath, std::string* error) const
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

        if (policy == ImageStoragePolicy::LinkOriginal)
        {
            policy = ImageStoragePolicy::SubfolderUnderNoteFolder;
        }
        if (policy == ImageStoragePolicy::EmbedInMarkdown)
        {
            if (error)
            {
                *error = "Embed clipboard images through CreateMarkdownImageFromClipboard.";
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

        std::vector<unsigned char> bytes;
        const bool bytesOk = ClipboardBitmapBytes(&bytes, error);
        CloseClipboard();
        if (!bytesOk)
        {
            return false;
        }

        const fs::path relative = AssetRelativePathForExtension(noteRelativePath, "clipboard", ".bmp", policy);
        const fs::path absolute = m_workspaceRoot / relative;
        if (!WriteBytes(absolute, bytes, error))
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
        (void)policy;
        (void)assetRelativePath;
        if (error)
        {
            *error = "Clipboard image paste is only implemented on Windows.";
        }
        return false;
#endif
    }

    bool AssetService::CreateMarkdownImageFromClipboard(const fs::path& noteRelativePath, ImageStoragePolicy policy,
                                                        std::string* markdown, std::string* error) const
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

        if (policy == ImageStoragePolicy::EmbedInMarkdown)
        {
            if (!OpenClipboard(nullptr))
            {
                if (error)
                {
                    *error = "Could not open clipboard.";
                }
                return false;
            }

            std::vector<unsigned char> bytes;
            const bool bytesOk = ClipboardBitmapBytes(&bytes, error);
            CloseClipboard();
            if (!bytesOk)
            {
                return false;
            }

            if (markdown)
            {
                *markdown = MarkdownImageTarget(DataUriForBytes(bytes, ".bmp"));
            }
            return true;
        }

        fs::path assetRelative;
        if (!SaveClipboardImageAsset(noteRelativePath, policy, &assetRelative, error))
        {
            return false;
        }
        if (markdown)
        {
            *markdown = MarkdownImageLink(noteRelativePath, assetRelative);
        }
        return true;
#else
        (void)noteRelativePath;
        (void)policy;
        (void)markdown;
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
        return MarkdownImageTarget(relative.generic_string());
    }

    std::string AssetService::MarkdownImageTarget(const std::string& target)
    {
        if (IsMarkdownUnsafeBareTarget(target) && target.rfind("data:", 0) != 0)
        {
            return "![](<" + target + ">)";
        }
        return "![](" + target + ")";
    }

    fs::path AssetService::AssetRelativePathForExtension(const fs::path& noteRelativePath, const std::string& stem,
                                                         const std::string& extension) const
    {
        return AssetRelativePathForExtension(noteRelativePath, stem, extension, ImageStoragePolicy::WorkspaceMediaFolder);
    }

    fs::path AssetService::AssetRelativePathForExtension(const fs::path& noteRelativePath, const std::string& stem,
                                                         const std::string& extension, ImageStoragePolicy policy) const
    {
        const std::string noteName = PathUtils::SanitizeFileName(noteRelativePath.stem().string());
        fs::path folder;
        switch (policy)
        {
        case ImageStoragePolicy::SameFolderAsNote:
            folder = noteRelativePath.parent_path();
            break;
        case ImageStoragePolicy::SubfolderUnderNoteFolder:
            folder = noteRelativePath.parent_path() / "attachments";
            break;
        case ImageStoragePolicy::WorkspaceMediaFolder:
        case ImageStoragePolicy::LinkOriginal:
        case ImageStoragePolicy::EmbedInMarkdown:
        default:
            folder = fs::path("assets") / noteName;
            break;
        }

        const std::string baseName = PathUtils::SanitizeFileName(stem);
        fs::path candidate = folder / (baseName + extension);
        for (int i = 2; fs::exists(m_workspaceRoot / candidate) && i < 1000; ++i)
        {
            candidate = folder / (baseName + "_" + std::to_string(i) + extension);
        }
        return PathUtils::NormalizeRelative(candidate);
    }
}
