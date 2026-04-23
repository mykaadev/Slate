#include "App/Slate/Markdown/MarkdownService.h"

#include "App/Slate/Core/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <sstream>
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

        bool IsTagNameChar(char ch)
        {
            const auto value = static_cast<unsigned char>(ch);
            return std::isalnum(value) != 0 || ch == '_' || ch == '-';
        }

        bool IsTagBoundary(char ch)
        {
            const auto value = static_cast<unsigned char>(ch);
            return std::isalnum(value) == 0 && ch != '_' && ch != '-';
        }

        bool IsIndentedContinuation(const std::string& line)
        {
            return StartsWith(line, "  ") || StartsWith(line, "\t");
        }

        std::string StripTodoContinuationIndent(const std::string& line)
        {
            if (StartsWith(line, "  "))
            {
                return line.substr(2);
            }
            if (StartsWith(line, "\t"))
            {
                return line.substr(1);
            }
            return line;
        }

        bool IsTodoLine(std::string_view trimmed)
        {
            return MarkdownService::ParseTodoLine(trimmed, nullptr, nullptr, nullptr);
        }

        void AddUniqueTag(std::vector<std::string>& tags, std::string tag)
        {
            if (tag.empty() || std::find(tags.begin(), tags.end(), tag) != tags.end())
            {
                return;
            }
            tags.push_back(std::move(tag));
        }

        void CollectTagsFromLine(std::string_view line, std::vector<std::string>& tags)
        {
            for (std::size_t i = 0; i < line.size(); ++i)
            {
                if (line[i] != '#')
                {
                    continue;
                }
                if (i > 0 && !IsTagBoundary(line[i - 1]))
                {
                    continue;
                }
                if (i + 1 >= line.size() || !IsTagNameChar(line[i + 1]))
                {
                    continue;
                }

                std::size_t end = i + 1;
                while (end < line.size() && IsTagNameChar(line[end]))
                {
                    ++end;
                }
                AddUniqueTag(tags, std::string(line.substr(i + 1, end - i - 1)));
                i = end;
            }
        }

        std::size_t LineOffsetForLine(const std::string& text, std::size_t targetLine)
        {
            if (targetLine <= 1)
            {
                return 0;
            }

            std::size_t line = 1;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == '\n')
                {
                    ++line;
                    if (line == targetLine)
                    {
                        return i + 1;
                    }
                }
            }
            return text.size();
        }

        std::size_t LineEndOffsetForLine(const std::string& text, std::size_t targetLine)
        {
            std::size_t line = 1;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (line == targetLine && text[i] == '\n')
                {
                    return i > 0 && text[i - 1] == '\r' ? i - 1 : i;
                }
                if (text[i] == '\n')
                {
                    ++line;
                }
            }
            return text.size();
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
                ParseTags(line, i + 1, offset, summary);
                ParseTodoTicket(lines, i, offset, summary);
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

    const char* MarkdownService::TodoStateLabel(TodoState state)
    {
        switch (state)
        {
        case TodoState::Open:
            return "Open";
        case TodoState::Research:
            return "Research";
        case TodoState::Doing:
            return "Doing";
        case TodoState::Done:
            return "Done";
        }
        return "Open";
    }

    TodoState MarkdownService::NextTodoState(TodoState state)
    {
        switch (state)
        {
        case TodoState::Open:
            return TodoState::Research;
        case TodoState::Research:
            return TodoState::Doing;
        case TodoState::Doing:
            return TodoState::Done;
        case TodoState::Done:
            return TodoState::Open;
        }
        return TodoState::Open;
    }

    bool MarkdownService::ParseTodoState(std::string_view text, TodoState* state)
    {
        const std::string value = PathUtils::ToLower(PathUtils::Trim(text));
        TodoState parsed = TodoState::Open;
        if (value == "open")
        {
            parsed = TodoState::Open;
        }
        else if (value == "research")
        {
            parsed = TodoState::Research;
        }
        else if (value == "doing")
        {
            parsed = TodoState::Doing;
        }
        else if (value == "done")
        {
            parsed = TodoState::Done;
        }
        else
        {
            return false;
        }

        if (state)
        {
            *state = parsed;
        }
        return true;
    }

    bool MarkdownService::ParseTodoLine(std::string_view text, TodoState* state, std::string* title, bool* done)
    {
        const std::string trimmed = PathUtils::Trim(text);
        if (trimmed.empty())
        {
            return false;
        }

        TodoState parsedState = TodoState::Open;
        bool parsedDone = false;
        std::size_t cursor = 0;

        if (StartsWith(trimmed, "- [ ] #todo "))
        {
            cursor = std::string("- [ ] #todo ").size();
        }
        else if (StartsWith(trimmed, "- [x] #todo ") || StartsWith(trimmed, "- [X] #todo "))
        {
            cursor = std::string("- [x] #todo ").size();
            parsedDone = true;
            parsedState = TodoState::Done;
        }
        else if (StartsWith(trimmed, "#todo"))
        {
            cursor = std::string("#todo").size();
            while (cursor < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0)
            {
                ++cursor;
            }
            if (cursor < trimmed.size() && trimmed[cursor] == '-')
            {
                ++cursor;
                while (cursor < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0)
                {
                    ++cursor;
                }
            }

            if (cursor >= trimmed.size() || trimmed[cursor] != '[')
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (cursor < trimmed.size() && trimmed[cursor] == '[')
        {
            const std::size_t close = trimmed.find(']', cursor + 1);
            if (close == std::string::npos)
            {
                return false;
            }

            if (!ParseTodoState(std::string_view(trimmed).substr(cursor + 1, close - cursor - 1), &parsedState))
            {
                return false;
            }

            cursor = close + 1;
            while (cursor < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0)
            {
                ++cursor;
            }
        }

        if (cursor < trimmed.size() && trimmed[cursor] == '-')
        {
            ++cursor;
            while (cursor < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0)
            {
                ++cursor;
            }
        }

        parsedDone = parsedDone || parsedState == TodoState::Done;

        if (state)
        {
            *state = parsedState;
        }
        if (title)
        {
            *title = PathUtils::Trim(std::string_view(trimmed).substr(std::min(cursor, trimmed.size())));
        }
        if (done)
        {
            *done = parsedDone;
        }
        return true;
    }

    std::string MarkdownService::FormatTodoBlock(TodoState state,
                                                 std::string_view title,
                                                 std::string_view description,
                                                 std::string_view lineEnding)
    {
        const std::string cleanTitle = PathUtils::Trim(title).empty() ? "Untitled todo" : PathUtils::Trim(title);
        const std::string cleanDescription = PathUtils::Trim(description);
        const std::string eol(lineEnding.empty() ? "\n" : lineEnding);
        std::string block = std::string("#todo - [") + TodoStateLabel(state) + "] - " + cleanTitle;

        if (!cleanDescription.empty())
        {
            std::istringstream stream(cleanDescription);
            std::string line;
            while (std::getline(stream, line))
            {
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                block += eol;
                block += "  ";
                block += line;
            }
        }
        return block;
    }

    bool MarkdownService::ReplaceTodoTicketBlock(const std::string& text,
                                                 const TodoTicket& ticket,
                                                 TodoState state,
                                                 std::string_view title,
                                                 std::string_view description,
                                                 std::string* updatedText)
    {
        if (!updatedText || ticket.line == 0 || ticket.endLine < ticket.line)
        {
            return false;
        }

        const std::size_t start = LineOffsetForLine(text, ticket.line);
        const std::size_t end = LineEndOffsetForLine(text, ticket.endLine);
        if (start > text.size() || end < start || end > text.size())
        {
            return false;
        }

        const std::string lineEnding = PathUtils::DetectLineEnding(text);
        std::string replacement = FormatTodoBlock(state, title, description, lineEnding);
        *updatedText = text.substr(0, start) + replacement + text.substr(end);
        return true;
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

    void MarkdownService::ParseTags(const std::string& line, std::size_t lineNumber, std::size_t lineOffset,
                                    MarkdownSummary& summary) const
    {
        std::size_t headingPrefix = 0;
        while (headingPrefix < line.size() && headingPrefix < 6 && line[headingPrefix] == '#')
        {
            ++headingPrefix;
        }
        if (headingPrefix > 0 && headingPrefix < line.size() &&
            std::isspace(static_cast<unsigned char>(line[headingPrefix])) != 0)
        {
            return;
        }
        else
        {
            headingPrefix = 0;
        }

        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (i < headingPrefix || line[i] != '#')
            {
                continue;
            }
            if (i > 0 && !IsTagBoundary(line[i - 1]))
            {
                continue;
            }
            if (i + 1 >= line.size() || !IsTagNameChar(line[i + 1]))
            {
                continue;
            }

            std::size_t end = i + 1;
            while (end < line.size() && IsTagNameChar(line[end]))
            {
                ++end;
            }

            MarkdownTag tag;
            tag.line = lineNumber;
            tag.start = lineOffset + i;
            tag.length = end - i;
            tag.name = line.substr(i + 1, end - i - 1);
            summary.tags.push_back(std::move(tag));
            i = end;
        }
    }

    std::size_t MarkdownService::ParseTodoTicket(const std::vector<std::string>& lines,
                                                 std::size_t index,
                                                 std::size_t,
                                                 MarkdownSummary& summary) const
    {
        if (index >= lines.size())
        {
            return index;
        }

        const std::string trimmed = PathUtils::Trim(lines[index]);
        if (!IsTodoLine(trimmed))
        {
            return index;
        }

        TodoTicket todo;
        todo.line = index + 1;
        if (!ParseTodoLine(trimmed, &todo.state, &todo.title, &todo.done))
        {
            return index;
        }

        std::vector<std::string> descriptionLines;
        std::size_t endLine = index + 1;
        for (std::size_t i = index + 1; i < lines.size(); ++i)
        {
            if (!IsIndentedContinuation(lines[i]))
            {
                break;
            }
            descriptionLines.push_back(StripTodoContinuationIndent(lines[i]));
            endLine = i + 1;
        }
        for (std::size_t i = 0; i < descriptionLines.size(); ++i)
        {
            if (i > 0)
            {
                todo.description += "\n";
            }
            todo.description += descriptionLines[i];
        }
        todo.description = PathUtils::Trim(todo.description);
        todo.endLine = endLine;

        for (const auto& tag : summary.tags)
        {
            if (tag.line >= todo.line && tag.line <= todo.endLine)
            {
                AddUniqueTag(todo.tags, tag.name);
            }
        }
        CollectTagsFromLine(trimmed, todo.tags);
        for (const auto& descriptionLine : descriptionLines)
        {
            CollectTagsFromLine(descriptionLine, todo.tags);
        }
        AddUniqueTag(todo.tags, "todo");

        summary.todos.push_back(std::move(todo));
        return endLine - 1;
    }
}
