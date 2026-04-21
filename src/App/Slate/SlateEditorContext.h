#pragma once

#include "App/Slate/DocumentService.h"
#include "App/Slate/EditorDocumentViewModel.h"
#include "App/Slate/JournalService.h"
#include "App/Slate/MarkdownService.h"
#include "App/Slate/WorkspaceService.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    class AssetService;

    class SlateEditorContext
    {
    public:
        MarkdownService& Markdown();
        const MarkdownService& Markdown() const;

        JournalService& Journal();
        const JournalService& Journal() const;

        EditorDocumentViewModel& Editor();
        const EditorDocumentViewModel& Editor() const;

        bool IsTextFocused() const;
        void SetTextFocused(bool focused);

        void LoadFromActiveDocument(const DocumentService& documents);
        void Clear();
        bool CommitToActiveDocument(DocumentService& documents, double elapsedSeconds);
        void JumpToLine(std::size_t line);
        void InsertTextAtCursor(DocumentService& documents, const std::string& text, double elapsedSeconds);
        std::vector<Heading> ParseHeadings(const DocumentService& documents) const;
        const JournalMonthSummary* CurrentMonthSummary(const WorkspaceService& workspace,
                                                       const DocumentService::Document* document,
                                                       double nowSeconds);
        void InvalidateJournalSummary();
        int ProcessDroppedFiles(std::vector<std::string>* droppedFiles, DocumentService& documents, AssetService& assets,
                                double elapsedSeconds, std::string* error = nullptr);

    private:
        MarkdownService m_markdown;
        JournalService m_journal;
        EditorDocumentViewModel m_editor;
        bool m_textFocused = false;
        bool m_journalSummaryValid = false;
        std::size_t m_journalSummaryTextHash = 0;
        double m_journalSummaryUpdatedSeconds = 0.0;
        fs::path m_journalSummaryPath;
        JournalMonthSummary m_journalSummary;
    };
}
