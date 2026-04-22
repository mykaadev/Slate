#include "App/Slate/SlateEditorContext.h"

#include "App/Slate/AssetService.h"
#include "App/Slate/ScintillaEditorHost.h"

#include <functional>

namespace Software::Slate
{
    SlateEditorContext::SlateEditorContext() : m_nativeEditor(std::make_unique<ScintillaEditorHost>())
    {
    }

    SlateEditorContext::~SlateEditorContext() = default;

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

    EditorDocumentViewModel& SlateEditorContext::Editor()
    {
        return m_editor;
    }

    const EditorDocumentViewModel& SlateEditorContext::Editor() const
    {
        return m_editor;
    }

    bool SlateEditorContext::IsTextFocused() const
    {
        if (m_nativeEditor && m_nativeEditor->Available())
        {
            return m_nativeEditor->IsFocused();
        }
        return m_textFocused;
    }

    void SlateEditorContext::SetTextFocused(bool focused)
    {
        m_textFocused = focused;
        if (!m_nativeEditor || !m_nativeEditor->Available())
        {
            return;
        }

        if (focused)
        {
            m_nativeEditor->Focus();
        }
        else
        {
            m_nativeEditor->ReleaseFocus();
        }
    }

    void SlateEditorContext::LoadFromActiveDocument(const DocumentService& documents)
    {
        if (const auto* document = documents.Active())
        {
            m_editor.Load(document->text, document->lineEnding);
            if (m_nativeEditor)
            {
                m_nativeEditor->LoadDocument(document, true);
                m_nativeEditor->Focus();
            }
            m_textFocused = true;
        }
        InvalidateJournalSummary();
    }

    void SlateEditorContext::Clear()
    {
        m_editor.Load("", "\n");
        if (m_nativeEditor)
        {
            m_nativeEditor->Clear();
            m_nativeEditor->SetVisible(false);
        }
        m_textFocused = false;
        InvalidateJournalSummary();
    }

    bool SlateEditorContext::CommitToActiveDocument(DocumentService& documents, double elapsedSeconds)
    {
        auto* document = documents.Active();
        if (!document)
        {
            return false;
        }

        if (m_nativeEditor && m_nativeEditor->Available())
        {
            return m_nativeEditor->SyncDocument(documents, elapsedSeconds);
        }

        std::string text;
        const bool changed = m_editor.CommitActiveLine(&text);
        if (changed || document->text != text)
        {
            document->text = std::move(text);
            documents.MarkEdited(elapsedSeconds);
            InvalidateJournalSummary();
            return true;
        }
        return false;
    }

    void SlateEditorContext::JumpToLine(std::size_t line)
    {
        if (line == 0)
        {
            return;
        }

        if (m_nativeEditor && m_nativeEditor->Available())
        {
            m_nativeEditor->JumpToLine(line);
        }
        m_editor.SetActiveLine(line - 1, 0);
        m_textFocused = true;
    }

    void SlateEditorContext::InsertTextAtCursor(DocumentService& documents, const std::string& text, double elapsedSeconds)
    {
        auto* document = documents.Active();
        if (!document || text.empty())
        {
            return;
        }

        m_editor.EnsureLoaded(document->text, document->lineEnding);
        if (m_nativeEditor && m_nativeEditor->Available())
        {
            if (m_nativeEditor->InsertTextAtCursor(text))
            {
                CommitToActiveDocument(documents, elapsedSeconds);
            }
            return;
        }

        if (m_editor.InsertTextAtCursor(text))
        {
            CommitToActiveDocument(documents, elapsedSeconds);
        }
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
                                                AssetService& assets, double elapsedSeconds, std::string* error)
    {
        if (!droppedFiles || droppedFiles->empty())
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
            droppedFiles->clear();
            return 0;
        }

        int inserted = 0;
        std::string lastError;
        for (const auto& pathText : *droppedFiles)
        {
            fs::path assetRelative;
            std::string itemError;
            if (assets.CopyImageAsset(document->relativePath, pathText, &assetRelative, &itemError))
            {
                InsertTextAtCursor(documents,
                                   AssetService::MarkdownImageLink(document->relativePath, assetRelative) +
                                       document->lineEnding,
                                   elapsedSeconds);
                ++inserted;
            }
            else
            {
                lastError = std::move(itemError);
            }
        }

        droppedFiles->clear();
        if (error)
        {
            *error = std::move(lastError);
        }
        return inserted;
    }

    void SlateEditorContext::AttachToNativeWindow(void* nativeHandle)
    {
        if (m_nativeEditor)
        {
            m_nativeEditor->AttachToParentWindow(nativeHandle);
        }
    }

    bool SlateEditorContext::NativeEditorAvailable() const
    {
        return m_nativeEditor && m_nativeEditor->Available();
    }

    bool SlateEditorContext::NativeEditorVisible() const
    {
        return m_nativeEditor && m_nativeEditor->Visible();
    }

    void SlateEditorContext::SetNativeEditorVisible(bool visible)
    {
        if (m_nativeEditor)
        {
            m_nativeEditor->SetVisible(visible);
        }
    }

    void SlateEditorContext::RenderNativeEditor(const DocumentService::Document& document,
                                                DocumentService& documents,
                                                double elapsedSeconds,
                                                float screenX,
                                                float screenY,
                                                float width,
                                                float height)
    {
        if (m_nativeEditor)
        {
            m_nativeEditor->Render(document, documents, elapsedSeconds, screenX, screenY, width, height);
            if (m_textFocused && m_nativeEditor->Visible() && !m_nativeEditor->IsFocused())
            {
                m_nativeEditor->Focus();
            }
        }
    }

    void SlateEditorContext::NotifyDocumentSaved()
    {
        if (m_nativeEditor)
        {
            m_nativeEditor->MarkSaved();
        }
    }

    bool SlateEditorContext::ConsumeNativeCommand(NativeEditorCommand command)
    {
        return m_nativeEditor && m_nativeEditor->ConsumeCommand(command);
    }

    void SlateEditorContext::ReleaseNativeEditorFocus()
    {
        if (m_nativeEditor)
        {
            m_nativeEditor->ReleaseFocus();
        }
        m_textFocused = false;
    }
}
