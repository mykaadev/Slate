#pragma once

#include "App/Slate/Documents/HistoryService.h"
#include "App/Slate/Core/SlateTypes.h"

#include <optional>
#include <string>

namespace Software::Slate
{
    // Owns the active document on disk and in memory
    class DocumentService
    {
    public:
        // Stores the active document state
        struct Document
        {
            // Stores the full path on disk
            fs::path absolutePath;
            // Stores the path relative to the workspace
            fs::path relativePath;
            // Stores the current document text
            std::string text;
            // Stores the line ending used for saves
            std::string lineEnding = "\n";
            // Marks unsaved editor changes
            bool dirty = false;
            // Marks a disk conflict with external edits
            bool conflict = false;
            // Marks whether a recovery snapshot exists
            bool recoveryAvailable = false;
            // Stores recovery text when present
            std::string recoveryText;
            // Stores the last write time seen on disk
            fs::file_time_type lastDiskWriteTime{};
            // Stores when the document was last edited
            double lastEditSeconds = 0.0;
            // Stores when the document was last saved
            double lastSaveSeconds = 0.0;
            // Stores when recovery was last written
            double lastRecoverySeconds = 0.0;
        };

        // Builds an empty document service
        DocumentService() = default;

        // Sets the workspace root used for document paths
        void SetWorkspaceRoot(fs::path root);
        // Opens a document by relative path
        bool Open(const fs::path& relativePath, std::string* error = nullptr);
        // Saves the active document
        bool Save(std::string* error = nullptr);
        // Saves only when the document is dirty
        bool SaveIfNeeded(double nowSeconds, std::string* error = nullptr);
        // Writes a recovery snapshot for the active document
        bool WriteRecovery(std::string* error = nullptr);
        // Restores the current recovery snapshot
        bool RestoreRecovery(std::string* error = nullptr);
        // Discards the current recovery snapshot
        bool DiscardRecovery(std::string* error = nullptr);
        // Marks the active document as edited
        void MarkEdited(double nowSeconds);
        // Checks whether the file changed on disk
        void CheckExternalChange();
        // Closes the active document
        void Close();

        // Reports whether a document is open
        bool HasOpenDocument() const;
        // Returns the active document
        Document* Active();
        // Returns the active document as const
        const Document* Active() const;

        // Builds the recovery path for a document
        fs::path RecoveryPathFor(const fs::path& relativePath) const;
        // Writes text to disk with an atomic swap
        static bool AtomicWriteText(const fs::path& target, const std::string& text, std::string* error = nullptr);
        // Reads a text file from disk
        static bool ReadTextFile(const fs::path& path, std::string* text, std::string* error = nullptr);

    private:
        // Resolves a workspace relative path
        fs::path Resolve(const fs::path& relativePath) const;
        // Captures a history snapshot before save
        bool SnapshotBeforeSave(std::string* error) const;
        // Clears the recovery file from disk
        bool ClearRecovery(std::string* error = nullptr) const;

        // Stores the active workspace root
        fs::path m_workspaceRoot;
        // Stores document history snapshots
        HistoryService m_history;
        // Stores the active document when one is open
        std::optional<Document> m_active;
    };
}
