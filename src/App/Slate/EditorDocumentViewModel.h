#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Software::Slate
{
    class EditorDocumentViewModel
    {
    public:
        void Load(std::string text, std::string lineEnding);
        void EnsureLoaded(std::string text, std::string lineEnding);
        bool CommitActiveLine(std::string* documentText = nullptr);
        std::string ToText() const;

        const std::vector<std::string>& Lines() const;
        bool Empty() const;

        std::size_t ActiveLine() const;
        const std::string& ActiveLineText() const;
        std::string& ActiveLineText();
        void ReplaceActiveLine(std::string text);
        void SetActiveLine(std::size_t line, int requestedCursor = -1);
        void MoveActiveLine(int delta);

        void SetCaretColumn(int column);
        int CaretColumn() const;
        int RequestedCursorColumn() const;
        void ClearRequestedCursorColumn();
        bool FocusRequested() const;
        void RequestFocus();
        void ClearFocusRequest();

        bool SplitActiveLine();
        bool MergeActiveLineWithPrevious();
        bool MergeActiveLineWithNext();
        bool InsertTextAtCursor(const std::string& text);

    private:
        void ClampActiveLine();
        void ReloadActiveLineText();
        static std::vector<std::string> SplitLines(const std::string& text);
        static std::string JoinLines(const std::vector<std::string>& lines, const std::string& lineEnding);

        std::vector<std::string> m_lines;
        std::string m_lineEnding = "\n";
        std::string m_activeLineText;
        std::size_t m_activeLine = 0;
        int m_caretColumn = 0;
        int m_desiredColumn = 0;
        int m_requestedCursorColumn = -1;
        bool m_focusRequested = false;
        bool m_loaded = false;
    };
}
