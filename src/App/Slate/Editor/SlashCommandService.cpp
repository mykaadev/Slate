#include "App/Slate/Editor/SlashCommandService.h"

#include "App/Slate/Core/PathUtils.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Software::Slate
{
    namespace
    {
        std::string TrimLeft(std::string_view value)
        {
            std::size_t start = 0;
            while (start < value.size() && (value[start] == ' ' || value[start] == '\t'))
            {
                ++start;
            }
            return std::string(value.substr(start));
        }
    }

    const std::vector<SlashCommandDefinition>& SlashCommandService::Commands()
    {
        static const std::vector<SlashCommandDefinition> commands{
            {"todo", "todo", "Create a todo file", SlashCommandKind::Todo},
            {"table", "table", "Insert a simple Markdown table", SlashCommandKind::Table},
            {"callout", "callout", "Insert a quoted callout block", SlashCommandKind::Callout},
            {"image", "image", "Insert or paste an image", SlashCommandKind::Image},
            {"code", "code block", "Insert a fenced code block", SlashCommandKind::CodeBlock},
            {"checklist", "checklist", "Insert a Markdown checklist item", SlashCommandKind::Checklist},
            {"quote", "quote", "Insert a block quote", SlashCommandKind::Quote},
            {"divider", "divider", "Insert a horizontal divider", SlashCommandKind::Divider},
            {"link", "link", "Insert a Markdown link", SlashCommandKind::Link},
        };
        return commands;
    }

    bool SlashCommandService::QueryFromLine(std::string_view line, std::string* query,
                                            std::size_t* tokenStart, std::size_t* tokenEnd)
    {
        if (line.empty())
        {
            return false;
        }

        const auto isBoundary = [](char ch) {
            return ch == ' ' || ch == '\t' || ch == '(' || ch == '[' || ch == '{';
        };

        for (std::size_t slash = line.size(); slash > 0;)
        {
            slash = line.rfind('/', slash - 1);
            if (slash == std::string_view::npos)
            {
                break;
            }

            if (slash > 0 && !isBoundary(line[slash - 1]))
            {
                continue;
            }

            const std::string_view body = line.substr(slash + 1);
            if (!body.empty() && (body.front() == ' ' || body.front() == '\t'))
            {
                return false;
            }
            if (body.find_first_of(" \t") != std::string_view::npos)
            {
                return false;
            }

            for (const char ch : body)
            {
                const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                   (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
                if (!valid)
                {
                    return false;
                }
            }

            if (query)
            {
                *query = PathUtils::ToLower(std::string(body));
            }
            if (tokenStart)
            {
                *tokenStart = slash;
            }
            if (tokenEnd)
            {
                *tokenEnd = line.size();
            }
            return true;
        }

        return false;
    }

    bool SlashCommandService::QueryFromLineAtCaret(std::string_view line,
                                                   std::size_t caretColumn,
                                                   std::string* query,
                                                   std::size_t* tokenStart,
                                                   std::size_t* tokenEnd)
    {
        caretColumn = std::min(caretColumn, line.size());
        const std::string_view prefix = line.substr(0, caretColumn);

        std::size_t prefixTokenStart = 0;
        std::size_t prefixTokenEnd = 0;
        if (!QueryFromLine(prefix, query, &prefixTokenStart, &prefixTokenEnd))
        {
            return false;
        }

        if (prefixTokenEnd != prefix.size())
        {
            return false;
        }

        if (tokenStart)
        {
            *tokenStart = prefixTokenStart;
        }
        if (tokenEnd)
        {
            *tokenEnd = caretColumn;
        }
        return true;
    }

    std::vector<SlashCommandDefinition> SlashCommandService::Filter(std::string_view query)
    {
        const std::string lowerQuery = PathUtils::ToLower(std::string(query));
        std::vector<SlashCommandDefinition> filtered;
        for (const auto& command : Commands())
        {
            const std::string id(command.id);
            const std::string label(command.label);
            const std::string description(command.description);
            if (lowerQuery.empty() ||
                PathUtils::ToLower(id).find(lowerQuery) != std::string::npos ||
                PathUtils::ToLower(label).find(lowerQuery) != std::string::npos ||
                PathUtils::ToLower(description).find(lowerQuery) != std::string::npos)
            {
                filtered.push_back(command);
            }
        }
        return filtered;
    }

    std::string SlashCommandService::BuildMarkdownTemplate(SlashCommandKind kind, std::string_view lineEnding)
    {
        const std::string eol(lineEnding.empty() ? "\n" : lineEnding);
        switch (kind)
        {
        case SlashCommandKind::Table:
            return "| Column | Column | Column |" + eol +
                   "| --- | --- | --- |" + eol +
                   "|  |  |  |";
        case SlashCommandKind::Callout:
            return "> [!note] Title" + eol + "> ";
        case SlashCommandKind::Image:
            return "![alt text](path/to/image.png)";
        case SlashCommandKind::CodeBlock:
            return "```" + eol + eol + "```";
        case SlashCommandKind::Checklist:
            return "- [ ] ";
        case SlashCommandKind::Quote:
            return "> ";
        case SlashCommandKind::Divider:
            return "---";
        case SlashCommandKind::Link:
            return "[label](url)";
        case SlashCommandKind::Todo:
            return "/todo";
        }
        return {};
    }
}
