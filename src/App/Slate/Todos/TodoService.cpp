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

        std::string PreserveTagsInTitle(std::string title, const TodoTicket& ticket)
        {
            title = PathUtils::Trim(title);
            for (const auto& tag : ticket.tags)
            {
                if (tag.empty() || tag == "todo")
                {
                    continue;
                }

                const std::string token = "#" + tag;
                if (!ContainsFilter(title, token))
                {
                    if (!title.empty())
                    {
                        title += " ";
                    }
                    title += token;
                }
            }
            return title;
        }

        void RefreshWorkspaceAfterTodoMutation(SlateWorkspaceContext& workspace)
        {
            workspace.Workspace().Scan(nullptr);
            workspace.Search().Rebuild(workspace.Workspace());
        }

        void PopulateResult(TodoMutationResult* result,
                            const fs::path& relativePath,
                            std::size_t revealLine,
                            bool touchedActiveDocument,
                            std::string error = {})
        {
            if (!result)
            {
                return;
            }

            result->relativePath = relativePath;
            result->revealLine = std::max<std::size_t>(1, revealLine);
            result->touchedActiveDocument = touchedActiveDocument;
            result->error = std::move(error);
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

    bool TodoService::UpdateTodo(SlateWorkspaceContext& workspace,
                                 const TodoTicket& ticket,
                                 TodoState state,
                                 std::string_view title,
                                 std::string_view description,
                                 double elapsedSeconds,
                                 TodoMutationResult* result) const
    {
        auto& documents = workspace.Documents();
        const auto normalized = PathUtils::NormalizeRelative(ticket.relativePath);
        auto* active = documents.Active();
        std::string updatedText;
        const std::string preservedTitle = PreserveTagsInTitle(std::string(title), ticket);

        if (active && PathUtils::NormalizeRelative(active->relativePath) == normalized)
        {
            if (!MarkdownService::ReplaceTodoTicketBlock(active->text,
                                                         ticket,
                                                         state,
                                                         preservedTitle,
                                                         description,
                                                         &updatedText))
            {
                PopulateResult(result, normalized, ticket.line, true, "could not update todo");
                return false;
            }

            active->text = std::move(updatedText);
            documents.MarkEdited(elapsedSeconds);
            PopulateResult(result, normalized, ticket.line, true);
            RefreshWorkspaceAfterTodoMutation(workspace);
            return true;
        }

        std::string text;
        std::string error;
        const auto absolutePath = workspace.Workspace().Resolve(normalized);
        if (!DocumentService::ReadTextFile(absolutePath, &text, &error))
        {
            PopulateResult(result, normalized, ticket.line, false, error);
            return false;
        }
        if (!MarkdownService::ReplaceTodoTicketBlock(text,
                                                     ticket,
                                                     state,
                                                     preservedTitle,
                                                     description,
                                                     &updatedText))
        {
            PopulateResult(result, normalized, ticket.line, false, "could not update todo");
            return false;
        }
        if (!DocumentService::AtomicWriteText(absolutePath, updatedText, &error))
        {
            PopulateResult(result, normalized, ticket.line, false, error);
            return false;
        }

        PopulateResult(result, normalized, ticket.line, false);
        RefreshWorkspaceAfterTodoMutation(workspace);
        return true;
    }

    bool TodoService::DeleteTodo(SlateWorkspaceContext& workspace,
                                 const TodoTicket& ticket,
                                 double elapsedSeconds,
                                 TodoMutationResult* result) const
    {
        auto& documents = workspace.Documents();
        const auto normalized = PathUtils::NormalizeRelative(ticket.relativePath);
        auto* active = documents.Active();
        std::string updatedText;
        const std::size_t revealLine = ticket.line > 1 ? ticket.line - 1 : 1;

        if (active && PathUtils::NormalizeRelative(active->relativePath) == normalized)
        {
            if (!MarkdownService::DeleteTodoTicketBlock(active->text, ticket, &updatedText))
            {
                PopulateResult(result, normalized, revealLine, true, "could not delete todo");
                return false;
            }

            active->text = std::move(updatedText);
            documents.MarkEdited(elapsedSeconds);
            PopulateResult(result, normalized, revealLine, true);
            RefreshWorkspaceAfterTodoMutation(workspace);
            return true;
        }

        std::string text;
        std::string error;
        const auto absolutePath = workspace.Workspace().Resolve(normalized);
        if (!DocumentService::ReadTextFile(absolutePath, &text, &error))
        {
            PopulateResult(result, normalized, revealLine, false, error);
            return false;
        }
        if (!MarkdownService::DeleteTodoTicketBlock(text, ticket, &updatedText))
        {
            PopulateResult(result, normalized, revealLine, false, "could not delete todo");
            return false;
        }
        if (!DocumentService::AtomicWriteText(absolutePath, updatedText, &error))
        {
            PopulateResult(result, normalized, revealLine, false, error);
            return false;
        }

        PopulateResult(result, normalized, revealLine, false);
        RefreshWorkspaceAfterTodoMutation(workspace);
        return true;
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
