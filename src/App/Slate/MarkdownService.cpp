#include "App/Slate/MarkdownService.h"

#include "App/Slate/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Software::Slate
{
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
