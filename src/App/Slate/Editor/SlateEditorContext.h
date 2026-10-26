#pragma once

#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/Live/LiveEditor.h"
#include "App/Slate/Markdown/JournalService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/Workspace/WorkspaceService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Software::Slate
{
    class AssetService;

    // Owns the editor side services for the active document
    class SlateEditorContext
    {
    public:
        // Returns the markdown parser service
        MarkdownService& Markdown();
        // Returns the markdown parser service as const
        const MarkdownService& Markdown() const;

        // Returns the journal helper service
        JournalService& Journal();
        // Returns the journal helper service as const
        const JournalService& Journal() const;

        // Returns the live Markdown editor
        Live::LiveEditor& Editor();
        // Returns the live Markdown editor as const
        const Live::LiveEditor& Editor() const;

        // Reports whether the editor currently owns the keyboard
        bool IsTextFocused() const;
        // Gives or takes keyboard focus
        void SetTextFocused(bool focused);
        // Takes keyboard focus away from the editor
        void ReleaseFocus();

        // Loads the active document into the editor
        void LoadFromActiveDocument(const DocumentService& documents);
        // Clears the editor
        void Clear();
        // Writes editor text back into the active document when it changed
        bool CommitToActiveDocument(DocumentService& documents, double elapsedSeconds);
        // Draws the editor for the active document and commits edits
        void DrawEditor(DocumentService& documents, const char* id, ImVec2 size, const Live::LiveEditorSettings& settings,
                        const Live::LiveEditorHooks& hooks, double elapsedSeconds);

        // Moves the caret to a 1-based source line
        void JumpToLine(std::size_t line);
        // Inserts text at the current cursor
        void InsertTextAtCursor(DocumentService& documents, const std::string& text, double elapsedSeconds);
        // Reads the current active line text
        bool ActiveLineText(std::string* text) const;
        // Reads the current active line text and caret column.
        bool ActiveLineTextAndCaret(std::string* text, std::size_t* caretColumn) const;
        // Reports whether the editor has selected text.
        bool HasSelection() const;
        // Reads the selected text
        bool SelectedText(std::string* text) const;
        // Reads the caret as 1-based line and byte column
        bool CaretPosition(std::size_t* line, std::size_t* column) const;
        // Replaces the whole document text as one undoable edit
        bool ReplaceAllText(DocumentService& documents, const std::string& text, double elapsedSeconds);
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
                                ImageStoragePolicy imageStoragePolicy, double elapsedSeconds,
                                std::string* error = nullptr);

    private:
        // Loads the active document when the editor shows a different file
        void EnsureLoaded(const DocumentService& documents);

        // Stores the markdown parser
        MarkdownService m_markdown;
        // Stores the journal helper
        JournalService m_journal;
        // Stores the live editor
        Live::LiveEditor m_editor;
        // Stores the editor revision last written into the document
        std::uint64_t m_committedRevision = 0;
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
