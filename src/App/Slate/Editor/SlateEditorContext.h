#pragma once

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/EditorDocumentViewModel.h"
#include "App/Slate/Markdown/JournalService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/Editor/ScintillaEditorHost.h"
#include "App/Slate/Workspace/WorkspaceService.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Software::Slate
{
    class AssetService;

    // Owns the editor side services for the active document
    class SlateEditorContext
    {
    public:
        // Builds the editor services
        SlateEditorContext();
        // Cleans up native editor resources
        ~SlateEditorContext();

        // Returns the markdown parser service
        MarkdownService& Markdown();
        // Returns the markdown parser service as const
        const MarkdownService& Markdown() const;

        // Returns the journal helper service
        JournalService& Journal();
        // Returns the journal helper service as const
        const JournalService& Journal() const;

        // Returns the line based editor model
        EditorDocumentViewModel& Editor();
        // Returns the line based editor model as const
        const EditorDocumentViewModel& Editor() const;

        // Reports whether text input currently owns focus
        bool IsTextFocused() const;
        // Updates text focus for the active editor path
        void SetTextFocused(bool focused);

        // Loads the active document into both editor paths
        void LoadFromActiveDocument(const DocumentService& documents);
        // Clears both editor paths
        void Clear();
        // Writes current editor text back into the active document
        bool CommitToActiveDocument(DocumentService& documents, double elapsedSeconds);
        // Moves the caret to a source line
        void JumpToLine(std::size_t line);
        // Inserts text at the current cursor
        void InsertTextAtCursor(DocumentService& documents, const std::string& text, double elapsedSeconds);
        // Reads the current active line text
        bool ActiveLineText(std::string* text) const;
        // Replaces the current line with a text block
        bool ReplaceActiveLineWithText(DocumentService& documents, const std::string& text, double elapsedSeconds);
        // Parses headings from the active document
        std::vector<Heading> ParseHeadings(const DocumentService& documents) const;
        // Returns the cached journal summary for the current note
        const JournalMonthSummary* CurrentMonthSummary(const WorkspaceService& workspace,
                                                       const DocumentService::Document* document,
                                                       double nowSeconds);
        // Clears cached journal data
        void InvalidateJournalSummary();
        // Imports dropped files into the current document
        int ProcessDroppedFiles(std::vector<std::string>* droppedFiles, DocumentService& documents, AssetService& assets,
                                double elapsedSeconds, std::string* error = nullptr);

        // Attaches the native editor host to the app window
        void AttachToNativeWindow(void* nativeHandle);
        // Reports whether the native editor can be used
        bool NativeEditorAvailable() const;
        // Reports whether the native editor is visible
        bool NativeEditorVisible() const;
        // Shows or hides the native editor
        void SetNativeEditorVisible(bool visible);
        // Applies editor settings to the native editor
        void SetNativeEditorSettings(const EditorSettings& settings);
        // Renders the native editor using a screen rectangle
        void RenderNativeEditor(const DocumentService::Document& document,
                                DocumentService& documents,
                                double elapsedSeconds,
                                float screenX,
                                float screenY,
                                float width,
                                float height);
        // Returns scroll state from the native editor
        ScintillaEditorHost::ScrollState NativeEditorScrollState() const;
        // Moves the native editor viewport to a line
        void SetNativeEditorFirstVisibleLine(int line);
        // Notifies the native editor that a save completed
        void NotifyDocumentSaved();
        // Pulls one pending command from the native editor
        bool ConsumeNativeCommand(NativeEditorCommand command);
        // Releases focus from the native editor
        void ReleaseNativeEditorFocus();

    private:
        // Stores the markdown parser
        MarkdownService m_markdown;
        // Stores the journal helper
        JournalService m_journal;
        // Stores the fallback line editor
        EditorDocumentViewModel m_editor;
        // Stores the native editor host when available
        std::unique_ptr<ScintillaEditorHost> m_nativeEditor;
        // Tracks focus for the fallback editor path
        bool m_textFocused = false;
        // Marks whether the cached journal summary is current
        bool m_journalSummaryValid = false;
        // Stores the hash used to validate the journal cache
        std::size_t m_journalSummaryTextHash = 0;
        // Stores when the journal cache was last refreshed
        double m_journalSummaryUpdatedSeconds = 0.0;
        // Stores the path tied to the journal cache
        fs::path m_journalSummaryPath;
        // Stores the cached journal summary
        JournalMonthSummary m_journalSummary;
    };
}
