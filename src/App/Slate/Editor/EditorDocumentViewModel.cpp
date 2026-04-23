#include "App/Slate/Editor/EditorDocumentViewModel.h"

#include "App/Slate/Markdown/MarkdownService.h"

#include <algorithm>
#include <utility>

namespace Software::Slate
{
    void EditorDocumentViewModel::Load(std::string text, std::string lineEnding)
    {
        m_lineEnding = std::move(lineEnding);
        m_lines = SplitLines(text);
        if (m_lines.empty())
        {
            m_lines.emplace_back();
        }
        m_activeLine = 0;
        m_caretColumn = 0;
        m_desiredColumn = 0;
        m_requestedCursorColumn = 0;
        m_focusRequested = true;
        m_loaded = true;
        ReloadActiveLineText();
    }

    void EditorDocumentViewModel::EnsureLoaded(std::string text, std::string lineEnding)
    {
        if (!m_loaded)
        {
            Load(std::move(text), std::move(lineEnding));
        }
    }

    bool EditorDocumentViewModel::CommitActiveLine(std::string* documentText)
    {
        ClampActiveLine();
        bool changed = false;
        if (m_lines[m_activeLine] != m_activeLineText)
        {
            m_lines[m_activeLine] = m_activeLineText;
            changed = true;
        }
        if (documentText)
        {
            *documentText = ToText();
        }
        return changed;
    }

    std::string EditorDocumentViewModel::ToText() const
    {
        std::vector<std::string> lines = m_lines;
        if (!lines.empty() && m_activeLine < lines.size())
        {
            lines[m_activeLine] = m_activeLineText;
        }
        return JoinLines(lines, m_lineEnding);
    }

    const std::vector<std::string>& EditorDocumentViewModel::Lines() const
    {
        return m_lines;
    }

    bool EditorDocumentViewModel::Empty() const
    {
        return m_lines.empty() || (m_lines.size() == 1 && m_lines.front().empty());
    }

    std::size_t EditorDocumentViewModel::ActiveLine() const
    {
        return m_activeLine;
    }

    const std::string& EditorDocumentViewModel::ActiveLineText() const
    {
        return m_activeLineText;
    }

    std::string& EditorDocumentViewModel::ActiveLineText()
    {
        return m_activeLineText;
    }

    void EditorDocumentViewModel::ReplaceActiveLine(std::string text)
    {
        m_activeLineText = std::move(text);
        m_caretColumn = std::clamp(m_caretColumn, 0, static_cast<int>(m_activeLineText.size()));
        m_desiredColumn = m_caretColumn;
    }

    bool EditorDocumentViewModel::ReplaceActiveLineWithText(const std::string& text)
    {
        ClampActiveLine();
        auto replacement = SplitLines(text);
        if (replacement.empty())
        {
            replacement.emplace_back();
        }

        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_activeLine));
        const std::size_t insertAt = m_activeLine;
        m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(insertAt),
                       replacement.begin(),
                       replacement.end());
        m_activeLine = insertAt + replacement.size() - 1;
        ReloadActiveLineText();
        m_caretColumn = static_cast<int>(m_activeLineText.size());
        m_desiredColumn = m_caretColumn;
        m_requestedCursorColumn = m_caretColumn;
        m_focusRequested = true;
        return true;
    }

    void EditorDocumentViewModel::SetActiveLine(std::size_t line, int requestedCursor)
    {
        CommitActiveLine();
        m_activeLine = line;
        ClampActiveLine();
        ReloadActiveLineText();
        const int target = requestedCursor < 0 ? static_cast<int>(m_activeLineText.size()) : requestedCursor;
        m_caretColumn = std::clamp(target, 0, static_cast<int>(m_activeLineText.size()));
        m_desiredColumn = m_caretColumn;
        m_requestedCursorColumn = m_caretColumn;
        m_focusRequested = true;
    }

    void EditorDocumentViewModel::MoveActiveLine(int delta)
    {
        CommitActiveLine();
        if (delta < 0)
        {
            const std::size_t amount = static_cast<std::size_t>(-delta);
            m_activeLine = amount > m_activeLine ? 0 : m_activeLine - amount;
        }
        else if (delta > 0)
        {
            m_activeLine = std::min(m_lines.size() - 1, m_activeLine + static_cast<std::size_t>(delta));
        }
        ReloadActiveLineText();
        m_caretColumn = std::min(m_desiredColumn, static_cast<int>(m_activeLineText.size()));
        m_requestedCursorColumn = m_caretColumn;
        m_focusRequested = true;
    }

    void EditorDocumentViewModel::SetCaretColumn(int column)
    {
        m_caretColumn = std::clamp(column, 0, static_cast<int>(m_activeLineText.size()));
        m_desiredColumn = m_caretColumn;
    }

    int EditorDocumentViewModel::CaretColumn() const
    {
        return m_caretColumn;
    }

    int EditorDocumentViewModel::RequestedCursorColumn() const
    {
        return m_requestedCursorColumn;
    }

    void EditorDocumentViewModel::ClearRequestedCursorColumn()
    {
        m_requestedCursorColumn = -1;
    }

    bool EditorDocumentViewModel::FocusRequested() const
    {
        return m_focusRequested;
    }

    void EditorDocumentViewModel::RequestFocus()
    {
        m_focusRequested = true;
    }

    void EditorDocumentViewModel::ClearFocusRequest()
    {
        m_focusRequested = false;
    }

    bool EditorDocumentViewModel::SplitActiveLine()
    {
        ClampActiveLine();
        const int cursor = std::clamp(m_caretColumn, 0, static_cast<int>(m_activeLineText.size()));
        const std::string left = m_activeLineText.substr(0, static_cast<std::size_t>(cursor));
        const std::string right = m_activeLineText.substr(static_cast<std::size_t>(cursor));
        m_lines[m_activeLine] = left;
        m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(m_activeLine + 1), right);
        ++m_activeLine;
        ReloadActiveLineText();
        m_caretColumn = 0;
        m_desiredColumn = 0;
        m_requestedCursorColumn = 0;
        m_focusRequested = true;
        return true;
    }

    bool EditorDocumentViewModel::MergeActiveLineWithPrevious()
    {
        ClampActiveLine();
        if (m_activeLine == 0)
        {
            return false;
        }
        const std::size_t previous = m_activeLine - 1;
        const int cursor = static_cast<int>(m_lines[previous].size());
        m_lines[previous] += m_activeLineText;
        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_activeLine));
        m_activeLine = previous;
        ReloadActiveLineText();
        m_caretColumn = cursor;
        m_desiredColumn = cursor;
        m_requestedCursorColumn = cursor;
        m_focusRequested = true;
        return true;
    }

    bool EditorDocumentViewModel::MergeActiveLineWithNext()
    {
        ClampActiveLine();
        if (m_activeLine + 1 >= m_lines.size())
        {
            return false;
        }
        const int cursor = static_cast<int>(m_activeLineText.size());
        m_activeLineText += m_lines[m_activeLine + 1];
        m_lines[m_activeLine] = m_activeLineText;
        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_activeLine + 1));
        m_caretColumn = cursor;
        m_desiredColumn = cursor;
        m_requestedCursorColumn = cursor;
        m_focusRequested = true;
        return true;
    }

    bool EditorDocumentViewModel::InsertTextAtCursor(const std::string& text)
    {
        if (text.empty())
        {
            return false;
        }

        ClampActiveLine();
        const int cursor = std::clamp(m_caretColumn, 0, static_cast<int>(m_activeLineText.size()));
        const std::string before = m_activeLineText.substr(0, static_cast<std::size_t>(cursor));
        const std::string after = m_activeLineText.substr(static_cast<std::size_t>(cursor));
        const auto replacement = SplitLines(before + text + after);

        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(m_activeLine));
        m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(m_activeLine), replacement.begin(),
                       replacement.end());

        const auto inserted = SplitLines(text);
        m_activeLine += inserted.empty() ? 0 : inserted.size() - 1;
        ClampActiveLine();
        ReloadActiveLineText();
        m_caretColumn = inserted.size() <= 1 ? static_cast<int>(before.size() + text.size())
                                             : static_cast<int>(inserted.back().size());
        m_caretColumn = std::clamp(m_caretColumn, 0, static_cast<int>(m_activeLineText.size()));
        m_desiredColumn = m_caretColumn;
        m_requestedCursorColumn = m_caretColumn;
        m_focusRequested = true;
        return true;
    }

    void EditorDocumentViewModel::ClampActiveLine()
    {
        if (m_lines.empty())
        {
            m_lines.emplace_back();
        }
        m_activeLine = std::min(m_activeLine, m_lines.size() - 1);
    }

    void EditorDocumentViewModel::ReloadActiveLineText()
    {
        ClampActiveLine();
        m_activeLineText = m_lines[m_activeLine];
        m_caretColumn = std::clamp(m_caretColumn, 0, static_cast<int>(m_activeLineText.size()));
        m_desiredColumn = m_caretColumn;
    }

    std::vector<std::string> EditorDocumentViewModel::SplitLines(const std::string& text)
    {
        auto lines = MarkdownService::SplitLines(text);
        if (lines.empty())
        {
            lines.emplace_back();
        }
        return lines;
    }

    std::string EditorDocumentViewModel::JoinLines(const std::vector<std::string>& lines, const std::string& lineEnding)
    {
        std::string text;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                text += lineEnding;
            }
            text += lines[i];
        }
        return text;
    }
}
