#include "App/Slate/MarkdownService.h"

#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
        }

        void AddSpan(std::vector<MarkdownInlineSpan>& spans,
                     std::string text,
                     bool bold,
                     bool italic,
                     bool code = false,
                     bool link = false,
                     bool image = false)
        {
            if (text.empty())
            {
                return;
            }

            if (!spans.empty() && spans.back().bold == bold && spans.back().italic == italic &&
                spans.back().code == code && spans.back().link == link && spans.back().image == image)
            {
                spans.back().text += text;
                return;
            }

            MarkdownInlineSpan span;
            span.text = std::move(text);
            span.bold = bold;
            span.italic = italic;
            span.code = code;
            span.link = link;
            span.image = image;
            spans.push_back(std::move(span));
        }

        void ParseInlineInto(std::string_view text, bool bold, bool italic, std::vector<MarkdownInlineSpan>& spans)
        {
            std::string plain;
            auto flushPlain = [&]() {
                AddSpan(spans, std::move(plain), bold, italic);
                plain.clear();
            };

            for (std::size_t i = 0; i < text.size();)
            {
                if (StartsWith(text.substr(i), "!["))
                {
                    const std::size_t closeLabel = text.find(']', i + 2);
                    if (closeLabel != std::string_view::npos && closeLabel + 1 < text.size() &&
                        text[closeLabel + 1] == '(')
                    {
                        const std::size_t closeTarget = text.find(')', closeLabel + 2);
                        if (closeTarget != std::string_view::npos)
                        {
                            flushPlain();
                            const std::string_view alt = text.substr(i + 2, closeLabel - i - 2);
                            const std::string_view target = text.substr(closeLabel + 2, closeTarget - closeLabel - 2);
                            std::string label = "[image";
                            if (!alt.empty())
                            {
                                label += ": ";
                                label += alt;
                            }
                            else if (!target.empty())
                            {
                                label += ": ";
                                label += fs::path(std::string(target)).filename().string();
                            }
                            label += "]";
                            AddSpan(spans, std::move(label), bold, italic, false, true, true);
                            i = closeTarget + 1;
                            continue;
                        }
                    }
                }

                if (StartsWith(text.substr(i), "[["))
                {
                    const std::size_t close = text.find("]]", i + 2);
                    if (close != std::string_view::npos)
                    {
                        flushPlain();
                        std::string inner(text.substr(i + 2, close - i - 2));
                        const std::size_t pipe = inner.find('|');
                        AddSpan(spans, pipe == std::string::npos ? inner : inner.substr(pipe + 1), bold, italic,
                                false, true);
                        i = close + 2;
                        continue;
                    }
                }

                if (text[i] == '[')
                {
                    const std::size_t closeLabel = text.find(']', i + 1);
                    if (closeLabel != std::string_view::npos && closeLabel + 1 < text.size() &&
                        text[closeLabel + 1] == '(')
                    {
                        const std::size_t closeTarget = text.find(')', closeLabel + 2);
                        if (closeTarget != std::string_view::npos)
                        {
                            flushPlain();
                            ParseInlineInto(text.substr(i + 1, closeLabel - i - 1), bold, italic, spans);
                            if (!spans.empty())
                            {
                                spans.back().link = true;
                            }
                            i = closeTarget + 1;
                            continue;
                        }
                    }
                }

                if (text[i] == '`')
                {
                    const std::size_t close = text.find('`', i + 1);
                    if (close != std::string_view::npos)
                    {
                        flushPlain();
                        AddSpan(spans, std::string(text.substr(i + 1, close - i - 1)), bold, italic, true);
                        i = close + 1;
                        continue;
                    }
                }

                if (StartsWith(text.substr(i), "**") || StartsWith(text.substr(i), "__"))
                {
                    const std::string_view marker = text.substr(i, 2);
                    const std::size_t close = text.find(marker, i + 2);
                    if (close != std::string_view::npos)
                    {
                        flushPlain();
                        ParseInlineInto(text.substr(i + 2, close - i - 2), true, italic, spans);
                        i = close + 2;
                        continue;
                    }
                }

                if ((text[i] == '*' || text[i] == '_') && i + 1 < text.size() && text[i + 1] != text[i])
                {
                    const char marker = text[i];
                    const std::size_t close = text.find(marker, i + 1);
                    if (close != std::string_view::npos)
                    {
                        flushPlain();
                        ParseInlineInto(text.substr(i + 1, close - i - 1), bold, true, spans);
                        i = close + 1;
                        continue;
                    }
                }

                plain.push_back(text[i]);
                ++i;
            }

            flushPlain();
        }
    }

    MarkdownSummary MarkdownService::Parse(const std::string& text) const
    {
        MarkdownSummary summary;
        const auto lines = SplitLines(text);
        bool inCodeFence = false;
        bool checkingFrontmatter = true;
        std::size_t offset = 0;
        auto advanceOffset = [&text](std::size_t currentOffset, const std::string& line) {
            std::size_t newlineLength = 0;
            if (currentOffset + line.size() < text.size())
            {
                newlineLength = 1;
                if (text[currentOffset + line.size()] == '\r' && currentOffset + line.size() + 1 < text.size() &&
                    text[currentOffset + line.size() + 1] == '\n')
                {
                    newlineLength = 2;
                }
            }
            return currentOffset + line.size() + newlineLength;
        };

        if (!lines.empty() && PathUtils::Trim(lines.front()) == "---")
        {
            summary.hasFrontmatter = true;
        }
        else
        {
            checkingFrontmatter = false;
        }

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const std::string& line = lines[i];
            const std::string trimmed = PathUtils::Trim(line);

            if (summary.hasFrontmatter && checkingFrontmatter && i > 0 && trimmed == "---")
            {
                summary.frontmatterEndLine = i;
                checkingFrontmatter = false;
                offset = advanceOffset(offset, line);
                continue;
            }

            if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0)
            {
                inCodeFence = !inCodeFence;
            }

            if (!inCodeFence && !checkingFrontmatter)
            {
                std::size_t hashes = 0;
                while (hashes < line.size() && hashes < 6 && line[hashes] == '#')
                {
                    ++hashes;
                }

                if (hashes > 0 && hashes < line.size() && std::isspace(static_cast<unsigned char>(line[hashes])))
                {
                    Heading heading;
                    heading.level = static_cast<int>(hashes);
                    heading.line = i + 1;
                    heading.title = PathUtils::Trim(std::string_view(line).substr(hashes));
                    summary.headings.push_back(std::move(heading));
                }

                ParseMarkdownLinks(line, i + 1, offset, summary);
                ParseWikiLinks(line, i + 1, offset, summary);
            }

            offset = advanceOffset(offset, line);
        }

        return summary;
    }

    std::vector<std::string> MarkdownService::SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start <= text.size())
        {
            const std::size_t end = text.find('\n', start);
            if (end == std::string::npos)
            {
                std::string line = text.substr(start);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                lines.push_back(std::move(line));
                break;
            }

            std::string line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            lines.push_back(std::move(line));
            start = end + 1;
        }
        return lines;
    }

    std::string MarkdownService::TitleFromText(const std::string& text, const fs::path& fallbackPath)
    {
        MarkdownService parser;
        const MarkdownSummary summary = parser.Parse(text);
        if (!summary.headings.empty())
        {
            return summary.headings.front().title;
        }
        return fallbackPath.stem().string();
    }

    std::vector<MarkdownInlineSpan> MarkdownService::ParseInlineSpans(std::string_view text)
    {
        std::vector<MarkdownInlineSpan> spans;
        ParseInlineInto(text, false, false, spans);
        return spans;
    }

    void MarkdownService::ParseMarkdownLinks(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                                             MarkdownSummary& summary) const
    {
        std::size_t pos = 0;
        while (pos < line.size())
        {
            const std::size_t labelOpen = line.find('[', pos);
            if (labelOpen == std::string::npos)
            {
                break;
            }

            const bool isImage = labelOpen > 0 && line[labelOpen - 1] == '!';
            const std::size_t labelClose = line.find(']', labelOpen + 1);
            if (labelClose == std::string::npos || labelClose + 1 >= line.size() || line[labelClose + 1] != '(')
            {
                pos = labelOpen + 1;
                continue;
            }

            const std::size_t targetClose = line.find(')', labelClose + 2);
            if (targetClose == std::string::npos)
            {
                break;
            }

            MarkdownLink link;
            link.line = lineNumber;
            link.start = lineOffset + (isImage ? labelOpen - 1 : labelOpen);
            link.length = targetClose + 1 - (isImage ? labelOpen - 1 : labelOpen);
            link.label = line.substr(labelOpen + 1, labelClose - labelOpen - 1);
            link.target = line.substr(labelClose + 2, targetClose - labelClose - 2);
            link.isImage = isImage;
            link.isWiki = false;
            summary.links.push_back(std::move(link));
            pos = targetClose + 1;
        }
    }

    void MarkdownService::ParseWikiLinks(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                                         MarkdownSummary& summary) const
    {
        std::size_t pos = 0;
        while (pos < line.size())
        {
            const std::size_t open = line.find("[[", pos);
            if (open == std::string::npos)
            {
                break;
            }

            const std::size_t close = line.find("]]", open + 2);
            if (close == std::string::npos)
            {
                break;
            }

            std::string inner = line.substr(open + 2, close - open - 2);
            const std::size_t pipe = inner.find('|');

            MarkdownLink link;
            link.line = lineNumber;
            link.start = lineOffset + open;
            link.length = close + 2 - open;
            link.target = pipe == std::string::npos ? inner : inner.substr(0, pipe);
            link.label = pipe == std::string::npos ? inner : inner.substr(pipe + 1);
            link.isWiki = true;
            link.isImage = false;
            summary.links.push_back(std::move(link));
            pos = close + 2;
        }
    }
}
