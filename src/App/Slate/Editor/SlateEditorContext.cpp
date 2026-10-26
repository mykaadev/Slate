#include "App/Slate/Editor/SlateEditorContext.h"

#include "App/Slate/Documents/AssetService.h"

#include <algorithm>
#include <functional>

namespace Software::Slate
{
    MarkdownService& SlateEditorContext::Markdown()
    {
        return m_markdown;
    }

    const MarkdownService& SlateEditorContext::Markdown() const
    {
        return m_markdown;
    }

    JournalService& SlateEditorContext::Journal()
    {
        return m_journal;
    }

    const JournalService& SlateEditorContext::Journal() const
    {
        return m_journal;
    }

    Live::LiveEditor& SlateEditorContext::Editor()
    {
        return m_editor;
    }

    const Live::LiveEditor& SlateEditorContext::Editor() const
    {
        return m_editor;
    }

    bool SlateEditorContext::IsTextFocused() const
    {
        return m_editor.HasDocument() && m_editor.Focused();
    }

    void SlateEditorContext::SetTextFocused(bool focused)
    {
        m_editor.SetFocused(focused);
    }

    void SlateEditorContext::ReleaseFocus()
    {
        m_editor.SetFocused(false);
    }

    void SlateEditorContext::LoadFromActiveDocument(const DocumentService& documents)
    {
        if (const auto* document = documents.Active())
        {
            m_editor.Load(document->text, document->lineEnding, document->absolutePath.u8string());
            m_committedRevision = m_editor.Revision();
            m_editor.SetFocused(true);
        }
        InvalidateJournalSummary();
    }

    void SlateEditorContext::EnsureLoaded(const DocumentService& documents)
    {
        const auto* document = documents.Active();
        if (document && m_editor.Key() != document->absolutePath.u8string())
        {
            LoadFromActiveDocument(documents);
        }
    }

    void SlateEditorContext::Clear()
    {
        m_editor.Clear();
        m_committedRevision = m_editor.Revision();
        InvalidateJournalSummary();
    }

    bool SlateEditorContext::CommitToActiveDocument(DocumentService& documents, double elapsedSeconds)
    {
        auto* document = documents.Active();
        if (!document || m_editor.Key() != document->absolutePath.u8string() || m_editor.Revision() == m_committedRevision)
        {
            return false;
        }

        m_committedRevision = m_editor.Revision();
        std::string text = m_editor.Text();
        if (text == document->text)
        {
            return false;
        }
        document->text = std::move(text);
        documents.MarkEdited(elapsedSeconds);
        InvalidateJournalSummary();
        return true;
    }

    void SlateEditorContext::DrawEditor(DocumentService& documents, const char* id, ImVec2 size,
                                        const Live::LiveEditorSettings& settings, const Live::LiveEditorHooks& hooks,
                                        double elapsedSeconds)
    {
        EnsureLoaded(documents);
        m_editor.Draw(id, size, settings, hooks, elapsedSeconds);
        CommitToActiveDocument(documents, elapsedSeconds);
    }

    void SlateEditorContext::JumpToLine(std::size_t line)
    {
        if (line == 0)
        {
            return;
        }
        m_editor.JumpToLine(line);
    }

    void SlateEditorContext::InsertTextAtCursor(DocumentService& documents, const std::string& text, double elapsedSeconds)
    {
        if (!documents.Active() || text.empty())
        {
            return;
        }
        EnsureLoaded(documents);
        m_editor.InsertText(text);
        CommitToActiveDocument(documents, elapsedSeconds);
    }

    bool SlateEditorContext::ActiveLineText(std::string* text) const
    {
        if (!text || !m_editor.HasDocument())
        {
            return false;
        }
        *text = m_editor.CurrentLine();
        return true;
    }

    bool SlateEditorContext::ActiveLineTextAndCaret(std::string* text, std::size_t* caretColumn) const
    {
        if (!ActiveLineText(text))
        {
            return false;
        }
        if (caretColumn)
        {
            *caretColumn = std::min(m_editor.CaretColumn(), text->size());
        }
        return true;
    }

    bool SlateEditorContext::HasSelection() const
    {
        return m_editor.HasSelection();
    }

    bool SlateEditorContext::SelectedText(std::string* text) const
    {
        return m_editor.SelectedText(text);
    }

    bool SlateEditorContext::CaretPosition(std::size_t* line, std::size_t* column) const
    {
        if (!m_editor.HasDocument())
        {
            return false;
        }
        if (line)
        {
            *line = m_editor.CaretLine() + 1;
        }
        if (column)
        {
            *column = m_editor.CaretColumn() + 1;
        }
        return true;
    }

    bool SlateEditorContext::ReplaceAllText(DocumentService& documents, const std::string& text, double elapsedSeconds)
    {
        if (!documents.Active())
        {
            return false;
        }
        EnsureLoaded(documents);
        m_editor.ReplaceAll(text);
        CommitToActiveDocument(documents, elapsedSeconds);
        InvalidateJournalSummary();
        return true;
    }

    bool SlateEditorContext::ReplaceActiveLineWithText(DocumentService& documents, const std::string& text, double elapsedSeconds)
    {
        if (!documents.Active())
        {
            return false;
        }
        EnsureLoaded(documents);
        m_editor.ReplaceCurrentLine(text);
        m_editor.SetFocused(true);
        CommitToActiveDocument(documents, elapsedSeconds);
        InvalidateJournalSummary();
        return true;
    }

    std::vector<Heading> SlateEditorContext::ParseHeadings(const DocumentService& documents) const
    {
        const auto* document = documents.Active();
        if (!document)
        {
            return {};
        }
        return m_markdown.Parse(document->text).headings;
    }

    const JournalMonthSummary* SlateEditorContext::CurrentMonthSummary(const WorkspaceService& workspace,
                                                                       const DocumentService::Document* document,
                                                                       double nowSeconds)
    {
        if (!document)
        {
            return nullptr;
        }

        const std::size_t textHash = std::hash<std::string>{}(document->text);
        const bool pathChanged = !m_journalSummaryValid || m_journalSummaryPath != document->relativePath;
        const bool textChanged = !m_journalSummaryValid || m_journalSummaryTextHash != textHash;
        const bool refreshDue = nowSeconds - m_journalSummaryUpdatedSeconds >= (textChanged ? 0.35 : 2.0);
        if (pathChanged || refreshDue)
        {
            int year = 0;
            int month = 0;
            int day = 0;
            if (m_journal.ParseJournalDate(document->relativePath, &year, &month, &day))
            {
                m_journalSummary = m_journal.BuildMonthSummary(workspace, year, month, document->relativePath,
                                                               &document->text);
            }
            else
            {
                m_journalSummary = m_journal.BuildCurrentMonthSummary(workspace, document->relativePath, &document->text);
            }
            m_journalSummaryPath = document->relativePath;
            m_journalSummaryTextHash = textHash;
            m_journalSummaryUpdatedSeconds = nowSeconds;
            m_journalSummaryValid = true;
        }

        return &m_journalSummary;
    }

    void SlateEditorContext::InvalidateJournalSummary()
    {
        m_journalSummaryValid = false;
    }

    int SlateEditorContext::ProcessDroppedFiles(std::vector<std::string>* droppedFiles, DocumentService& documents,
                                                AssetService& assets, ImageStoragePolicy imageStoragePolicy,
                                                double elapsedSeconds, std::string* error)
    {
        std::vector<std::string> pendingDrops;
        if (droppedFiles && !droppedFiles->empty())
        {
            pendingDrops.insert(pendingDrops.end(), droppedFiles->begin(), droppedFiles->end());
            droppedFiles->clear();
        }
        if (pendingDrops.empty())
        {
            return 0;
        }

        auto* document = documents.Active();
        if (!document)
        {
            if (error)
            {
                *error = "open a note before dropping image files";
            }
            return 0;
        }

        int inserted = 0;
        std::string lastError;
        for (const auto& pathText : pendingDrops)
        {
            std::string markdown;
            std::string itemError;
            if (assets.CreateMarkdownImageFromFile(document->relativePath, pathText, imageStoragePolicy, &markdown,
                                                   &itemError))
            {
                InsertTextAtCursor(documents, markdown + "\n", elapsedSeconds);
                ++inserted;
            }
            else
            {
                lastError = std::move(itemError);
            }
        }

        if (error)
        {
            *error = std::move(lastError);
        }
        return inserted;
    }
}
