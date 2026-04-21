#pragma once

#include "App/Slate/SlateTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    class MarkdownService
    {
    public:
        MarkdownSummary Parse(const std::string& text) const;

        static std::vector<std::string> SplitLines(const std::string& text);
        static std::string TitleFromText(const std::string& text, const fs::path& fallbackPath);
        static std::vector<MarkdownInlineSpan> ParseInlineSpans(std::string_view text);

    private:
        void ParseMarkdownLinks(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                                MarkdownSummary& summary) const;
        void ParseWikiLinks(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                            MarkdownSummary& summary) const;
    };
}
