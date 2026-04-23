#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Software::Slate
{
    // Keeps an editable line based view of the active document
    class EditorDocumentViewModel
    {
    public:
        // Loads fresh text into the editor model
        void Load(std::string text, std::string lineEnding);
        // Loads text only when the model is not already current
        void EnsureLoaded(std::string text, std::string lineEnding);
        // Writes the active line back into the line buffer
        bool CommitActiveLine(std::string* documentText = nullptr);
        // Joins the line buffer into full document text
        std::string ToText() const;

        // Returns the current line buffer
        const std::vector<std::string>& Lines() const;
        // Reports whether the buffer has any visible content
        bool Empty() const;

        // Returns the active line index
        std::size_t ActiveLine() const;
        // Returns the active line as read only text
        const std::string& ActiveLineText() const;
        // Returns the active line as editable text
        std::string& ActiveLineText();
        // Replaces the active line text in memory
        void ReplaceActiveLine(std::string text);
        // Replaces the active line with a multi line block
        bool ReplaceActiveLineWithText(const std::string& text);
        // Changes the active line and optional cursor target
        void SetActiveLine(std::size_t line, int requestedCursor = -1);
        // Moves the active line by a signed delta
        void MoveActiveLine(int delta);

        // Stores the current caret column
        void SetCaretColumn(int column);
        // Returns the current caret column
        int CaretColumn() const;
        // Returns the requested cursor column for the next focus pass
        int RequestedCursorColumn() const;
        // Clears the requested cursor column
        void ClearRequestedCursorColumn();
        // Reports whether the editor wants keyboard focus
        bool FocusRequested() const;
        // Requests keyboard focus for the active line
        void RequestFocus();
        // Clears the pending focus request
        void ClearFocusRequest();

        // Splits the active line at the caret
        bool SplitActiveLine();
        // Merges the active line into the previous line
        bool MergeActiveLineWithPrevious();
        // Merges the next line into the active line
        bool MergeActiveLineWithNext();
        // Inserts raw text at the caret
        bool InsertTextAtCursor(const std::string& text);

    private:
        // Keeps the active line inside valid bounds
        void ClampActiveLine();
        // Refreshes the editable copy of the active line
        void ReloadActiveLineText();
        // Splits a full document into lines
        static std::vector<std::string> SplitLines(const std::string& text);
        // Joins lines into full document text
        static std::string JoinLines(const std::vector<std::string>& lines, const std::string& lineEnding);

        // Stores the line buffer for the document
        std::vector<std::string> m_lines;
        // Stores the line ending used when joining text
        std::string m_lineEnding = "\n";
        // Stores the editable copy of the active line
        std::string m_activeLineText;
        // Stores the active line index
        std::size_t m_activeLine = 0;
        // Stores the current caret column
        int m_caretColumn = 0;
        // Stores the preferred column during vertical moves
        int m_desiredColumn = 0;
        // Stores the next requested cursor column
        int m_requestedCursorColumn = -1;
        // Marks that the editor wants focus next frame
        bool m_focusRequested = false;
        // Marks whether the model already has document text
        bool m_loaded = false;
    };
}
