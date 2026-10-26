#include "App/Slate/Editor/Live/MarkdownBlocks.h"

#include <algorithm>
#include <cctype>

namespace Software::Slate::Live
{
    namespace
    {
        bool IsSpace(char ch)
        {
            return ch == ' ' || ch == '\t';
        }

        std::string_view Trim(std::string_view text)
        {
            while (!text.empty() && IsSpace(text.front()))
            {
                text.remove_prefix(1);
            }
            while (!text.empty() && IsSpace(text.back()))
            {
                text.remove_suffix(1);
            }
            return text;
        }

        bool FenceOpen(std::string_view line, char* fenceChar, std::size_t* length, std::string* info)
        {
            std::size_t pos = 0;
            while (pos < line.size() && pos < 3 && line[pos] == ' ')
            {
                ++pos;
            }
            if (pos >= line.size() || (line[pos] != '`' && line[pos] != '~'))
            {
                return false;
            }
            const char ch = line[pos];
            std::size_t count = 0;
            while (pos + count < line.size() && line[pos + count] == ch)
            {
                ++count;
            }
            if (count < 3)
            {
                return false;
            }
            *fenceChar = ch;
            *length = count;
            if (info)
            {
                std::string_view rest = Trim(line.substr(pos + count));
                rest = rest.substr(0, rest.find_first_of(" \t{"));
                info->assign(rest);
                std::transform(info->begin(), info->end(), info->begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
            }
            return true;
        }

        bool FenceCloses(std::string_view line, char fenceChar, std::size_t length)
        {
            const std::string_view trimmed = Trim(line);
            return trimmed.size() >= length && trimmed.find_first_not_of(fenceChar) == std::string_view::npos;
        }

        bool IsTableRow(std::string_view line)
        {
            const std::string_view trimmed = Trim(line);
            return !trimmed.empty() && trimmed.find('|') != std::string_view::npos && SplitTableRow(trimmed).size() >= 2;
        }

        bool ParseCalloutHeader(std::string_view line, Block* block)
        {
            const std::size_t content = QuoteContentOffset(line);
            if (content == 0)
            {
                return false;
            }
            std::string_view rest = line.substr(content);
            if (rest.rfind("[!", 0) != 0)
            {
                return false;
            }
            const std::size_t close = rest.find(']');
            if (close == std::string_view::npos || close <= 2)
            {
                return false;
            }
            block->calloutType.assign(rest.substr(2, close - 2));
            std::transform(block->calloutType.begin(), block->calloutType.end(), block->calloutType.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            rest = rest.substr(close + 1);
            if (!rest.empty() && (rest.front() == '+' || rest.front() == '-'))
            {
                block->calloutFold = rest.front();
                rest.remove_prefix(1);
            }
            block->calloutTitle.assign(Trim(rest));
            return true;
        }
    }

    std::vector<Block> ParseBlocks(const std::vector<std::string_view>& lines)
    {
        std::vector<Block> blocks;
        std::size_t i = 0;

        // Frontmatter only counts at the very top of the file
        if (!lines.empty() && Trim(lines[0]) == "---")
        {
            for (std::size_t end = 1; end < lines.size(); ++end)
            {
                const std::string_view trimmed = Trim(lines[end]);
                if (trimmed == "---" || trimmed == "...")
                {
                    Block block;
                    block.kind = BlockKind::Frontmatter;
                    block.firstLine = 0;
                    block.lastLine = end;
                    blocks.push_back(block);
                    i = end + 1;
                    break;
                }
            }
        }

        while (i < lines.size())
        {
            const std::string_view line = lines[i];
            Block block;
            block.firstLine = i;
            block.lastLine = i;

            char fenceChar = 0;
            std::size_t fenceLength = 0;
            if (FenceOpen(line, &fenceChar, &fenceLength, &block.language))
            {
                block.kind = BlockKind::Code;
                std::size_t end = i + 1;
                while (end < lines.size() && !FenceCloses(lines[end], fenceChar, fenceLength))
                {
                    ++end;
                }
                block.unclosed = end >= lines.size();
                block.lastLine = std::min(end, lines.size() - 1);
                blocks.push_back(block);
                i = block.lastLine + 1;
                continue;
            }

            if (Trim(line) == "$$")
            {
                block.kind = BlockKind::Math;
                std::size_t end = i + 1;
                while (end < lines.size() && Trim(lines[end]) != "$$")
                {
                    ++end;
                }
                block.unclosed = end >= lines.size();
                block.lastLine = std::min(end, lines.size() - 1);
                blocks.push_back(block);
                i = block.lastLine + 1;
                continue;
            }

            if (i + 1 < lines.size() && IsTableRow(line) && IsTableDelimiter(lines[i + 1]))
            {
                block.kind = BlockKind::Table;
                std::size_t end = i + 2;
                while (end < lines.size() && IsTableRow(lines[end]))
                {
                    ++end;
                }
                block.lastLine = end - 1;
                blocks.push_back(block);
                i = end;
                continue;
            }

            if (ParseCalloutHeader(line, &block))
            {
                block.kind = BlockKind::Callout;
                std::size_t end = i + 1;
                while (end < lines.size() && QuoteContentOffset(lines[end]) > 0)
                {
                    ++end;
                }
                block.lastLine = end - 1;
                blocks.push_back(block);
                i = end;
                continue;
            }

            blocks.push_back(block);
            ++i;
        }
        return blocks;
    }

    std::vector<TableCell> SplitTableRow(std::string_view line)
    {
        std::size_t start = 0;
        std::size_t end = line.size();
        while (start < end && IsSpace(line[start]))
        {
            ++start;
        }
        while (end > start && IsSpace(line[end - 1]))
        {
            --end;
        }
        if (start < end && line[start] == '|')
        {
            ++start;
        }
        if (end > start && line[end - 1] == '|' && (end - 1 == start || line[end - 2] != '\\'))
        {
            --end;
        }

        std::vector<TableCell> cells;
        std::size_t cellStart = start;
        for (std::size_t i = start; i <= end; ++i)
        {
            if (i == end || (line[i] == '|' && (i == start || line[i - 1] != '\\')))
            {
                std::size_t a = cellStart;
                std::size_t b = i;
                while (a < b && IsSpace(line[a]))
                {
                    ++a;
                }
                while (b > a && IsSpace(line[b - 1]))
                {
                    --b;
                }
                cells.push_back({line.substr(a, b - a), a});
                cellStart = i + 1;
            }
        }
        return cells;
    }

    bool IsTableDelimiter(std::string_view line)
    {
        const auto cells = SplitTableRow(line);
        if (cells.empty())
        {
            return false;
        }
        for (const auto& cell : cells)
        {
            std::string_view text = cell.text;
            if (!text.empty() && text.front() == ':')
            {
                text.remove_prefix(1);
            }
            if (!text.empty() && text.back() == ':')
            {
                text.remove_suffix(1);
            }
            if (text.empty() || text.find_first_not_of('-') != std::string_view::npos)
            {
                return false;
            }
        }
        return true;
    }

    std::vector<TableAlign> TableAlignments(std::string_view delimiter)
    {
        std::vector<TableAlign> aligns;
        for (const auto& cell : SplitTableRow(delimiter))
        {
            const bool left = !cell.text.empty() && cell.text.front() == ':';
            const bool right = !cell.text.empty() && cell.text.back() == ':';
            aligns.push_back(left && right ? TableAlign::Center : (right ? TableAlign::Right : TableAlign::Left));
        }
        return aligns;
    }

    std::size_t QuoteContentOffset(std::string_view line)
    {
        std::size_t pos = 0;
        while (pos < line.size() && pos < 3 && line[pos] == ' ')
        {
            ++pos;
        }
        if (pos >= line.size() || line[pos] != '>')
        {
            return 0;
        }
        ++pos;
        if (pos < line.size() && line[pos] == ' ')
        {
            ++pos;
        }
        return pos;
    }

    std::string CalloutDefaultTitle(std::string_view type)
    {
        std::string title(type);
        if (!title.empty())
        {
            title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
        }
        return title;
    }
}
