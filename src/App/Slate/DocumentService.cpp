#include "App/Slate/DocumentService.h"

#include "App/Slate/PathUtils.h"

#include <fstream>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace Software::Slate
{
    void DocumentService::SetWorkspaceRoot(fs::path root)
    {
        m_workspaceRoot = std::move(root);
        m_history.SetWorkspaceRoot(m_workspaceRoot);
    }

    bool DocumentService::Open(const fs::path& relativePath, std::string* error)
    {
        const fs::path absolute = Resolve(PathUtils::NormalizeRelative(relativePath));
        std::string text;
        if (!ReadTextFile(absolute, &text, error))
        {
            return false;
        }

        Document document;
        document.absolutePath = absolute;
        document.relativePath = PathUtils::NormalizeRelative(relativePath);
        document.text = std::move(text);
        document.lineEnding = PathUtils::DetectLineEnding(document.text);

        std::error_code ec;
        document.lastDiskWriteTime = fs::last_write_time(absolute, ec);
        document.dirty = false;
        document.conflict = false;

        const fs::path recoveryPath = RecoveryPathFor(document.relativePath);
        if (fs::exists(recoveryPath, ec))
        {
            const auto recoveryTime = fs::last_write_time(recoveryPath, ec);
            if (!ec && recoveryTime > document.lastDiskWriteTime)
            {
                document.recoveryAvailable = ReadTextFile(recoveryPath, &document.recoveryText, nullptr);
            }
        }

        m_active = std::move(document);
        return true;
    }

    bool DocumentService::Save(std::string* error)
    {
        if (!m_active)
        {
            return true;
        }

        CheckExternalChange();
        if (m_active->conflict)
        {
            if (error)
            {
                *error = "File changed on disk. Resolve the conflict before saving.";
            }
            return false;
        }

        if (!SnapshotBeforeSave(error))
        {
            return false;
        }

        if (!AtomicWriteText(m_active->absolutePath, m_active->text, error))
        {
            return false;
        }

        std::error_code ec;
        m_active->lastDiskWriteTime = fs::last_write_time(m_active->absolutePath, ec);
        m_active->dirty = false;
        m_active->conflict = false;
        ClearRecovery(nullptr);
        return true;
    }

    bool DocumentService::SaveIfNeeded(double nowSeconds, std::string* error)
    {
        if (!m_active || !m_active->dirty || m_active->conflict)
        {
            return true;
        }

        if (nowSeconds - m_active->lastRecoverySeconds >= 1.0)
        {
            WriteRecovery(nullptr);
            m_active->lastRecoverySeconds = nowSeconds;
        }

        if (nowSeconds - m_active->lastEditSeconds >= 2.0 && nowSeconds - m_active->lastSaveSeconds >= 2.0)
        {
            if (!Save(error))
            {
                return false;
            }
            m_active->lastSaveSeconds = nowSeconds;
        }
        return true;
    }

    bool DocumentService::WriteRecovery(std::string* error)
    {
        if (!m_active || !m_active->dirty)
        {
            return true;
        }

        const fs::path recoveryPath = RecoveryPathFor(m_active->relativePath);
        std::error_code ec;
        fs::create_directories(recoveryPath.parent_path(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        return AtomicWriteText(recoveryPath, m_active->text, error);
    }

    bool DocumentService::RestoreRecovery(std::string* error)
    {
        if (!m_active || !m_active->recoveryAvailable)
        {
            return true;
        }

        m_active->text = m_active->recoveryText;
        m_active->dirty = true;
        m_active->recoveryAvailable = false;
        return WriteRecovery(error);
    }

    bool DocumentService::DiscardRecovery(std::string* error)
    {
        if (!m_active)
        {
            return true;
        }
        m_active->recoveryAvailable = false;
        m_active->recoveryText.clear();
        return ClearRecovery(error);
    }

    void DocumentService::MarkEdited(double nowSeconds)
    {
        if (!m_active)
        {
            return;
        }
        m_active->dirty = true;
        m_active->lastEditSeconds = nowSeconds;
    }

    void DocumentService::CheckExternalChange()
    {
        if (!m_active)
        {
            return;
        }

        std::error_code ec;
        const auto currentWriteTime = fs::last_write_time(m_active->absolutePath, ec);
        if (!ec && currentWriteTime != m_active->lastDiskWriteTime)
        {
            m_active->conflict = m_active->dirty;
            if (!m_active->dirty)
            {
                std::string text;
                if (ReadTextFile(m_active->absolutePath, &text, nullptr))
                {
                    m_active->text = std::move(text);
                    m_active->lastDiskWriteTime = currentWriteTime;
                }
            }
        }
    }

    void DocumentService::Close()
    {
        m_active.reset();
    }

    bool DocumentService::HasOpenDocument() const
    {
        return m_active.has_value();
    }

    DocumentService::Document* DocumentService::Active()
    {
        return m_active ? &*m_active : nullptr;
    }

    const DocumentService::Document* DocumentService::Active() const
    {
        return m_active ? &*m_active : nullptr;
    }

    fs::path DocumentService::RecoveryPathFor(const fs::path& relativePath) const
    {
        return m_workspaceRoot / ".slate" / "recovery" /
               (PathUtils::StablePathId(relativePath) + "_" + PathUtils::SanitizeFileName(relativePath.filename().string()));
    }

    bool DocumentService::AtomicWriteText(const fs::path& target, const std::string& text, std::string* error)
    {
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        const fs::path temp = target.parent_path() / (target.filename().string() + ".slate-tmp");
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                if (error)
                {
                    *error = "Could not write temporary file.";
                }
                return false;
            }
            file << text;
            file.flush();
            if (!file)
            {
                if (error)
                {
                    *error = "Could not flush temporary file.";
                }
                return false;
            }
        }

#if defined(_WIN32)
        if (!MoveFileExW(temp.wstring().c_str(), target.wstring().c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            if (error)
            {
                *error = "Could not replace target file.";
            }
            fs::remove(temp, ec);
            return false;
        }
#else
        fs::rename(temp, target, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            fs::remove(temp, ec);
            return false;
        }
#endif
        return true;
    }

    bool DocumentService::ReadTextFile(const fs::path& path, std::string* text, std::string* error)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            if (error)
            {
                *error = "Could not read file.";
            }
            return false;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        *text = ss.str();
        return true;
    }

    fs::path DocumentService::Resolve(const fs::path& relativePath) const
    {
        return (m_workspaceRoot / relativePath).lexically_normal();
    }

    bool DocumentService::SnapshotBeforeSave(std::string* error) const
    {
        if (!m_active)
        {
            return true;
        }
        return m_history.SnapshotFile(m_active->absolutePath, m_active->relativePath, error);
    }

    bool DocumentService::ClearRecovery(std::string* error) const
    {
        if (!m_active)
        {
            return true;
        }

        std::error_code ec;
        fs::remove(RecoveryPathFor(m_active->relativePath), ec);
        if (ec && error)
        {
            *error = ec.message();
            return false;
        }
        return true;
    }
}
