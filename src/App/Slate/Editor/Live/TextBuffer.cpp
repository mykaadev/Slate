#include "App/Slate/Editor/Live/TextBuffer.h"

#include <algorithm>
#include <cctype>

namespace Software::Slate::Live
{
    namespace
    {
        constexpr std::size_t MaxUndoSteps = 1000;
        constexpr double TypingGroupSeconds = 1.2;

        bool IsContinuationByte(char ch)
        {
            return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
        }

        // Word characters include UTF-8 bytes so accented words move as one
        bool IsWordByte(char ch)
        {
            const auto byte = static_cast<unsigned char>(ch);
            return std::isalnum(byte) != 0 || ch == '_' || byte >= 0x80;
        }
    }

    void TextBuffer::SetText(std::string_view text)
    {
        m_text.clear();
        m_text.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\r')
            {
                m_text.push_back('\n');
                if (i + 1 < text.size() && text[i + 1] == '\n')
                {
                    ++i;
                }
                continue;
            }
            m_text.push_back(text[i]);
        }
        m_undo.clear();
        m_redo.clear();
        m_breakGroup = true;
        RebuildLines();
        ++m_revision;
    }

    std::string TextBuffer::TextWithLineEnding(std::string_view lineEnding) const
    {
        if (lineEnding.empty() || lineEnding == "\n")
        {
            return m_text;
        }
        std::string result;
        result.reserve(m_text.size() + m_lineStarts.size() * lineEnding.size());
        for (const char ch : m_text)
        {
            if (ch == '\n')
            {
                result.append(lineEnding);
            }
            else
            {
                result.push_back(ch);
            }
        }
        return result;
    }

    std::size_t TextBuffer::LineStart(std::size_t line) const
    {
        return line < m_lineStarts.size() ? m_lineStarts[line] : m_text.size();
    }

    std::size_t TextBuffer::LineEnd(std::size_t line) const
    {
        if (line + 1 < m_lineStarts.size())
        {
            return m_lineStarts[line + 1] - 1;
        }
        return m_text.size();
    }

    std::size_t TextBuffer::LineOfOffset(std::size_t offset) const
    {
        const auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
        return static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, (it - m_lineStarts.begin()) - 1));
    }

    std::string_view TextBuffer::Line(std::size_t line) const
    {
        const std::size_t start = LineStart(line);
        return std::string_view(m_text).substr(start, LineEnd(line) - start);
    }

    std::size_t TextBuffer::PrevChar(std::size_t offset) const
    {
        if (offset == 0)
        {
            return 0;
        }
        --offset;
        while (offset > 0 && IsContinuationByte(m_text[offset]))
        {
            --offset;
        }
        return offset;
    }

    std::size_t TextBuffer::NextChar(std::size_t offset) const
    {
        if (offset >= m_text.size())
        {
            return m_text.size();
        }
        ++offset;
        while (offset < m_text.size() && IsContinuationByte(m_text[offset]))
        {
            ++offset;
        }
        return offset;
    }

    std::size_t TextBuffer::PrevWord(std::size_t offset) const
    {
        offset = std::min(offset, m_text.size());
        while (offset > 0 && (m_text[offset - 1] == ' ' || m_text[offset - 1] == '\t'))
        {
            --offset;
        }
        if (offset > 0 && m_text[offset - 1] == '\n')
        {
            return offset - 1;
        }
        if (offset > 0 && !IsWordByte(m_text[offset - 1]))
        {
            return PrevChar(offset);
        }
        while (offset > 0 && IsWordByte(m_text[offset - 1]))
        {
            --offset;
        }
        return offset;
    }

    std::size_t TextBuffer::NextWord(std::size_t offset) const
    {
        offset = std::min(offset, m_text.size());
        if (offset < m_text.size() && m_text[offset] == '\n')
        {
            return offset + 1;
        }
        if (offset < m_text.size() && !IsWordByte(m_text[offset]) && m_text[offset] != ' ' && m_text[offset] != '\t')
        {
            offset = NextChar(offset);
        }
        else
        {
            while (offset < m_text.size() && IsWordByte(m_text[offset]))
            {
                ++offset;
            }
        }
        while (offset < m_text.size() && (m_text[offset] == ' ' || m_text[offset] == '\t'))
        {
            ++offset;
        }
        return offset;
    }

    void TextBuffer::Replace(std::size_t start, std::size_t end, std::string_view text,
                             const Selection& before, const Selection& after, EditKind kind, double now)
    {
        start = std::min(start, m_text.size());
        end = std::clamp(end, start, m_text.size());
        if (start == end && text.empty())
        {
            return;
        }

        Change change;
        change.start = start;
        change.removed = m_text.substr(start, end - start);
        change.inserted = std::string(text);

        // Consecutive typing or deleting merges into one undo step until a pause, a newline, or a jump
        bool merged = false;
        if (!m_breakGroup && !m_undo.empty() && kind != EditKind::Other && m_undo.back().kind == kind &&
            now - m_undo.back().time < TypingGroupSeconds && text.find('\n') == std::string_view::npos)
        {
            Step& last = m_undo.back();
            const Change& previous = last.changes.back();
            const bool contiguousTyping = kind == EditKind::Typing && change.removed.empty() &&
                                          previous.start + previous.inserted.size() == start;
            const bool contiguousDelete = kind == EditKind::Deleting && change.inserted.empty() &&
                                          (start + change.removed.size() == previous.start || start == previous.start);
            if (contiguousTyping || contiguousDelete)
            {
                last.changes.push_back(change);
                last.after = after;
                last.time = now;
                merged = true;
            }
        }

        if (!merged)
        {
            Step step;
            step.changes.push_back(change);
            step.before = before;
            step.after = after;
            step.kind = kind;
            step.time = now;
            m_undo.push_back(std::move(step));
            if (m_undo.size() > MaxUndoSteps)
            {
                m_undo.erase(m_undo.begin());
            }
        }

        m_redo.clear();
        m_breakGroup = kind == EditKind::Other || text.find('\n') != std::string_view::npos;
        ApplyRaw(start, end, text);
    }

    void TextBuffer::BreakUndoGroup()
    {
        m_breakGroup = true;
    }

    bool TextBuffer::Undo(Selection* selection)
    {
        if (m_undo.empty())
        {
            return false;
        }
        Step step = std::move(m_undo.back());
        m_undo.pop_back();
        for (auto it = step.changes.rbegin(); it != step.changes.rend(); ++it)
        {
            ApplyRaw(it->start, it->start + it->inserted.size(), it->removed);
        }
        if (selection)
        {
            *selection = step.before;
        }
        m_redo.push_back(std::move(step));
        m_breakGroup = true;
        return true;
    }

    bool TextBuffer::Redo(Selection* selection)
    {
        if (m_redo.empty())
        {
            return false;
        }
        Step step = std::move(m_redo.back());
        m_redo.pop_back();
        for (const auto& change : step.changes)
        {
            ApplyRaw(change.start, change.start + change.removed.size(), change.inserted);
        }
        if (selection)
        {
            *selection = step.after;
        }
        m_undo.push_back(std::move(step));
        m_breakGroup = true;
        return true;
    }

    void TextBuffer::ApplyRaw(std::size_t start, std::size_t end, std::string_view text)
    {
        m_text.replace(start, end - start, text);
        RebuildLines();
        ++m_revision;
    }

    void TextBuffer::RebuildLines()
    {
        m_lineStarts.assign(1, 0);
        for (std::size_t i = 0; i < m_text.size(); ++i)
        {
            if (m_text[i] == '\n')
            {
                m_lineStarts.push_back(i + 1);
            }
        }
    }
}
