#pragma once

#include "App/Slate/HistoryService.h"
#include "App/Slate/SlateTypes.h"

#include <optional>
#include <string>

namespace Software::Slate
{
    class DocumentService
    {
    public:
        struct Document
        {
            fs::path absolutePath;
            fs::path relativePath;
            std::string text;
            std::string lineEnding = "\n";
            bool dirty = false;
            bool conflict = false;
            bool recoveryAvailable = false;
            std::string recoveryText;
            fs::file_time_type lastDiskWriteTime{};
            double lastEditSeconds = 0.0;
            double lastSaveSeconds = 0.0;
            double lastRecoverySeconds = 0.0;
        };

        DocumentService() = default;

        void SetWorkspaceRoot(fs::path root);
        bool Open(const fs::path& relativePath, std::string* error = nullptr);
        bool Save(std::string* error = nullptr);
        bool SaveIfNeeded(double nowSeconds, std::string* error = nullptr);
        bool WriteRecovery(std::string* error = nullptr);
        bool RestoreRecovery(std::string* error = nullptr);
        bool DiscardRecovery(std::string* error = nullptr);
        void MarkEdited(double nowSeconds);
        void CheckExternalChange();
        void Close();

        bool HasOpenDocument() const;
        Document* Active();
        const Document* Active() const;

        fs::path RecoveryPathFor(const fs::path& relativePath) const;
        static bool AtomicWriteText(const fs::path& target, const std::string& text, std::string* error = nullptr);
        static bool ReadTextFile(const fs::path& path, std::string* text, std::string* error = nullptr);

    private:
        fs::path Resolve(const fs::path& relativePath) const;
        bool SnapshotBeforeSave(std::string* error) const;
        bool ClearRecovery(std::string* error = nullptr) const;

        fs::path m_workspaceRoot;
        HistoryService m_history;
        std::optional<Document> m_active;
    };
}
