#include "App/Slate/Todos/TodoService.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        std::string LowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool ContainsFilter(const std::string& value, const std::string& filter)
        {
            if (filter.empty())
            {
                return true;
            }
            return LowerCopy(value).find(LowerCopy(filter)) != std::string::npos;
        }
    }

    std::vector<TodoTicket> TodoService::CollectTodos(const SlateWorkspaceContext& workspace) const
    {
        std::vector<TodoTicket> todos;
        MarkdownService markdown;
        const auto* activeDocument = workspace.Documents().Active();
        const auto activePath = activeDocument ? PathUtils::NormalizeRelative(activeDocument->relativePath)
                                               : fs::path{};

        for (const auto& relativePath : workspace.Workspace().MarkdownFiles())
        {
            std::string text;
            const auto normalized = PathUtils::NormalizeRelative(relativePath);
            if (activeDocument && normalized == activePath)
            {
                text = activeDocument->text;
            }
            else if (!DocumentService::ReadTextFile(workspace.Workspace().Resolve(relativePath), &text, nullptr))
            {
                continue;
            }

            auto summary = markdown.Parse(text);
            for (auto& todo : summary.todos)
            {
                todo.relativePath = relativePath;
                todos.push_back(std::move(todo));
            }
        }

        std::sort(todos.begin(), todos.end(), [](const auto& left, const auto& right) {
            const int leftState = TodoService::StateSortKey(left.state);
            const int rightState = TodoService::StateSortKey(right.state);
            if (leftState != rightState)
            {
                return leftState < rightState;
            }
            if (left.relativePath != right.relativePath)
            {
                return left.relativePath.generic_string() < right.relativePath.generic_string();
            }
            return left.line < right.line;
        });
        return todos;
    }

    int TodoService::StateSortKey(TodoState state)
    {
        switch (state)
        {
        case TodoState::Open:
            return 0;
        case TodoState::Research:
            return 1;
        case TodoState::Doing:
            return 2;
        case TodoState::Done:
            return 3;
        }
        return 0;
    }

    bool TodoService::MatchesQuery(const TodoTicket& todo, const std::string& query)
    {
        if (query.empty())
        {
            return true;
        }

        if (ContainsFilter(todo.title, query) ||
            ContainsFilter(todo.description, query) ||
            ContainsFilter(todo.relativePath.generic_string(), query) ||
            ContainsFilter(MarkdownService::TodoStateLabel(todo.state), query))
        {
            return true;
        }

        for (const auto& tag : todo.tags)
        {
            const std::string needle = "#" + tag;
            if (ContainsFilter(needle, query) || ContainsFilter(tag, query))
            {
                return true;
            }
        }
        return false;
    }
}
