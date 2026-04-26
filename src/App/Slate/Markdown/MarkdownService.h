#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    // Parses markdown into editor friendly structures
    class MarkdownService
    {
    public:
        // Parses one full markdown document
        MarkdownSummary Parse(const std::string& text) const;

        // Splits raw text into logical lines
        static std::vector<std::string> SplitLines(const std::string& text);
        // Picks a title from markdown or the fallback path
        static std::string TitleFromText(const std::string& text, const fs::path& fallbackPath);
        // Parses inline markdown styling spans
        static std::vector<MarkdownInlineSpan> ParseInlineSpans(std::string_view text);
        // Returns the visible label for a todo state
        static const char* TodoStateLabel(TodoState state);
        // Cycles a todo state to the next state
        static TodoState NextTodoState(TodoState state);
        // Parses a todo state label
        static bool ParseTodoState(std::string_view text, TodoState* state);
        // Parses a todo line in either modern or legacy format
        static bool ParseTodoLine(std::string_view text,
                                  TodoState* state,
                                  std::string* title = nullptr,
                                  bool* done = nullptr);
    private:
        // Parses standard markdown links
        void ParseMarkdownLinks(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                                MarkdownSummary& summary) const;
        // Parses wiki style links
        void ParseWikiLinks(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                            MarkdownSummary& summary) const;
        // Parses hash tags outside blocked regions
        void ParseTags(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                       MarkdownSummary& summary) const;
        // Parses a legacy inline todo block and returns the last consumed line
        std::size_t ParseTodoTicket(const std::vector<std::string>& lines,
                                    std::size_t index,
                                    std::size_t lineOffset,
                                    MarkdownSummary& summary) const;
    };
}
