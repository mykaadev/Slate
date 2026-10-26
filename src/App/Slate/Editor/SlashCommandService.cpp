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
        return Filter(query, {});
    }

    std::vector<SlashCommandDefinition> SlashCommandService::Filter(std::string_view query,
                                                                    const std::vector<SlashCommandDefinition>& extensions)
    {
        const std::string lowerQuery = PathUtils::ToLower(std::string(query));
        const auto matches = [&lowerQuery](const SlashCommandDefinition& command) {
            return lowerQuery.empty() ||
                   PathUtils::ToLower(command.id).find(lowerQuery) != std::string::npos ||
                   PathUtils::ToLower(command.label).find(lowerQuery) != std::string::npos ||
                   PathUtils::ToLower(command.description).find(lowerQuery) != std::string::npos;
        };

        std::vector<SlashCommandDefinition> filtered;
        for (const auto& command : Commands())
        {
            if (matches(command))
            {
                filtered.push_back(command);
            }
        }
        for (const auto& command : extensions)
        {
            const bool shadowsBuiltIn = std::any_of(Commands().begin(), Commands().end(), [&](const SlashCommandDefinition& builtIn) {
                return builtIn.id == command.id;
            });
            if (!shadowsBuiltIn && matches(command))
            {
                filtered.push_back(command);
            }
        }

        // Exact and prefix matches float to the top so typing the full token always selects it
        std::stable_sort(filtered.begin(), filtered.end(), [&lowerQuery](const SlashCommandDefinition& lhs,
                                                                         const SlashCommandDefinition& rhs) {
            const auto rank = [&lowerQuery](const SlashCommandDefinition& command) {
                const std::string id = PathUtils::ToLower(command.id);
                if (id == lowerQuery)
                {
                    return 0;
                }
                return id.rfind(lowerQuery, 0) == 0 ? 1 : 2;
            };
            return rank(lhs) < rank(rhs);
        });
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
        case SlashCommandKind::Extension:
            return {};
        }
        return {};
    }
}
