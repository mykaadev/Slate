#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate::Live
{
    // A caret plus the anchor of its selection, as byte offsets into the buffer
    struct Selection
    {
        std::size_t anchor = 0;
        std::size_t caret = 0;

        std::size_t Min() const { return anchor < caret ? anchor : caret; }
        std::size_t Max() const { return anchor < caret ? caret : anchor; }
        bool Empty() const { return anchor == caret; }
        static Selection At(std::size_t offset) { return {offset, offset}; }
    };

    // How an edit groups with the previous one in the undo history
    enum class EditKind
    {
        Typing,
        Deleting,
        Other
    };

    // UTF-8 text with "\n" line endings, a line index, and undo history
    class TextBuffer
    {
    public:
        // Replaces all text and clears history; "\r\n" and "\r" become "\n"
        void SetText(std::string_view text);
        // Returns the text with "\n" line endings
        const std::string& Text() const { return m_text; }
        // Returns the text with the requested line ending
        std::string TextWithLineEnding(std::string_view lineEnding) const;

        std::size_t Size() const { return m_text.size(); }
        std::size_t LineCount() const { return m_lineStarts.size(); }
        std::size_t LineStart(std::size_t line) const;
        // Returns the offset just before the line's "\n" (or the buffer end)
        std::size_t LineEnd(std::size_t line) const;
        std::size_t LineOfOffset(std::size_t offset) const;
        std::string_view Line(std::size_t line) const;

        // Moves an offset to the previous or next UTF-8 code point boundary
        std::size_t PrevChar(std::size_t offset) const;
        std::size_t NextChar(std::size_t offset) const;
        // Moves an offset to the previous or next word boundary
        std::size_t PrevWord(std::size_t offset) const;
        std::size_t NextWord(std::size_t offset) const;

        // Replaces [start, end) with text and records the change for undo
        void Replace(std::size_t start, std::size_t end, std::string_view text,
                     const Selection& before, const Selection& after, EditKind kind, double now);
        // Starts a new undo step even if the next edit could merge
        void BreakUndoGroup();
        bool CanUndo() const { return !m_undo.empty(); }
        bool CanRedo() const { return !m_redo.empty(); }
        bool Undo(Selection* selection);
        bool Redo(Selection* selection);
        // Changes on every edit, undo, and redo
        std::uint64_t Revision() const { return m_revision; }

    private:
        struct Change
        {
            std::size_t start = 0;
            std::string removed;
            std::string inserted;
        };

        struct Step
        {
            std::vector<Change> changes;
            Selection before;
            Selection after;
            EditKind kind = EditKind::Other;
            double time = 0.0;
        };

        void ApplyRaw(std::size_t start, std::size_t end, std::string_view text);
        void RebuildLines();

        std::string m_text;
        std::vector<std::size_t> m_lineStarts{0};
        std::vector<Step> m_undo;
        std::vector<Step> m_redo;
        bool m_breakGroup = true;
        std::uint64_t m_revision = 1;
    };
}
