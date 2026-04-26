#include "App/Slate/Todos/TodoService.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <system_error>
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

        bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
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

        void RefreshWorkspaceAfterTodoMutation(SlateWorkspaceContext& workspace)
        {
            workspace.Workspace().Scan(nullptr);
            workspace.Search().Rebuild(workspace.Workspace());
        }

        std::string CleanTitle(std::string_view title)
        {
            std::string clean = PathUtils::Trim(title);
            return clean.empty() ? "Untitled todo" : clean;
        }

        std::string CleanDescription(std::string_view description)
        {
            std::string clean = PathUtils::Trim(description);
            return clean.empty() ? "Description" : clean;
        }

        std::string Slugify(std::string_view value)
        {
            std::string slug;
            bool previousDash = false;
            for (unsigned char c : std::string(value))
            {
                if (std::isalnum(c))
                {
                    slug.push_back(static_cast<char>(std::tolower(c)));
                    previousDash = false;
                }
                else if (!previousDash)
                {
                    slug.push_back('-');
                    previousDash = true;
                }
            }
            while (!slug.empty() && slug.front() == '-')
            {
                slug.erase(slug.begin());
            }
            while (!slug.empty() && slug.back() == '-')
            {
                slug.pop_back();
            }
            return slug.empty() ? "todo" : slug;
        }

        std::vector<std::string> SplitPreservingLines(std::string_view text)
        {
            std::vector<std::string> lines;
            std::string current;
            for (const char ch : text)
            {
                if (ch == '\r')
                {
                    continue;
                }
                if (ch == '\n')
                {
                    lines.push_back(current);
                    current.clear();
                }
                else
                {
                    current.push_back(ch);
                }
            }
            lines.push_back(current);
            return lines;
        }

        bool TryReadFrontmatter(const std::vector<std::string>& lines,
                                std::string* type,
                                TodoState* state,
                                TodoPriority* priority,
                                std::size_t* frontmatterEnd)
        {
            if (lines.empty() || PathUtils::Trim(lines.front()) != "---")
            {
                return false;
            }

            TodoState parsedState = TodoState::Open;
            TodoPriority parsedPriority = TodoPriority::Normal;
            std::string parsedType;
            for (std::size_t i = 1; i < lines.size(); ++i)
            {
                const std::string trimmed = PathUtils::Trim(lines[i]);
                if (trimmed == "---")
                {
                    if (type)
                    {
                        *type = parsedType;
                    }
                    if (state)
                    {
                        *state = parsedState;
                    }
                    if (priority)
                    {
                        *priority = parsedPriority;
                    }
                    if (frontmatterEnd)
                    {
                        *frontmatterEnd = i;
                    }
                    return true;
                }

                const auto colon = trimmed.find(':');
                if (colon == std::string::npos)
                {
                    continue;
                }
                const std::string key = PathUtils::ToLower(PathUtils::Trim(trimmed.substr(0, colon)));
                std::string value = PathUtils::Trim(trimmed.substr(colon + 1));
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                {
                    value = value.substr(1, value.size() - 2);
                }

                if (key == "slate" || key == "type" || key == "slate.type")
                {
                    parsedType = PathUtils::ToLower(value);
                }
                else if (key == "status" || key == "state")
                {
                    MarkdownService::ParseTodoState(value, &parsedState);
                }
                else if (key == "priority")
                {
                    TodoService::ParsePriority(value, &parsedPriority);
                }
            }
            return false;
        }

        std::size_t FindTitleLine(const std::vector<std::string>& lines, std::size_t startIndex)
        {
            for (std::size_t i = startIndex; i < lines.size(); ++i)
            {
                const std::string trimmed = PathUtils::Trim(lines[i]);
                if (StartsWith(trimmed, "# "))
                {
                    return i;
                }
            }
            return lines.size();
        }

        std::vector<std::string> TagsFromText(std::string_view text)
        {
            MarkdownService markdown;
            auto summary = markdown.Parse(std::string(text));
            std::vector<std::string> tags;
            for (const auto& tag : summary.tags)
            {
                if (std::find(tags.begin(), tags.end(), tag.name) == tags.end())
                {
                    tags.push_back(tag.name);
                }
            }
            return tags;
        }
    }

    std::vector<TodoTicket> TodoService::CollectTodos(const SlateWorkspaceContext& workspace) const
    {
        std::vector<TodoTicket> todos;
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

            TodoTicket todo;
            if (!ParseTodoDocument(text, normalized, &todo))
            {
                continue;
            }
            todos.push_back(std::move(todo));
        }

        std::sort(todos.begin(), todos.end(), [](const auto& left, const auto& right) {
            const int leftState = TodoService::StateSortKey(left.state);
            const int rightState = TodoService::StateSortKey(right.state);
            if (leftState != rightState)
            {
                return leftState < rightState;
            }
            const int leftPriority = TodoService::PrioritySortKey(left.priority);
            const int rightPriority = TodoService::PrioritySortKey(right.priority);
            if (leftPriority != rightPriority)
            {
                return leftPriority > rightPriority;
            }
            return left.relativePath.generic_string() < right.relativePath.generic_string();
        });
        return todos;
    }

    bool TodoService::CreateTodo(SlateWorkspaceContext& workspace,
                                 TodoState state,
                                 std::string_view title,
                                 std::string_view description,
                                 double elapsedSeconds,
                                 TodoMutationResult* result,
                                 TodoPriority priority) const
    {
        const std::string text = FormatTodoDocument(state, title, description, "\n", priority);
        const fs::path relativePath = MakeTodoPath(workspace, title);
        fs::path created;
        std::string error;
        if (!workspace.Workspace().CreateMarkdownFile(relativePath, text, &created, &error))
        {
            PopulateResult(result, relativePath, 1, false, error);
            return false;
        }

        workspace.Workspace().TouchRecent(created);
        (void)elapsedSeconds;
        PopulateResult(result, created, 7, false);
        RefreshWorkspaceAfterTodoMutation(workspace);
        return true;
    }

    bool TodoService::UpdateTodo(SlateWorkspaceContext& workspace,
                                 const TodoTicket& ticket,
                                 TodoState state,
                                 std::string_view title,
                                 std::string_view description,
                                 double elapsedSeconds,
                                 TodoMutationResult* result,
                                 TodoPriority priority) const
    {
        auto& documents = workspace.Documents();
        const auto normalized = PathUtils::NormalizeRelative(ticket.relativePath);
        auto* active = documents.Active();

        if (active && PathUtils::NormalizeRelative(active->relativePath) == normalized)
        {
            const std::string updatedText = FormatTodoDocument(state, title, description, active->lineEnding, priority);
            active->text = updatedText;
            documents.MarkEdited(elapsedSeconds);
            PopulateResult(result, normalized, 7, true);
            RefreshWorkspaceAfterTodoMutation(workspace);
            return true;
        }

        std::string existingText;
        std::string error;
        const auto absolutePath = workspace.Workspace().Resolve(normalized);
        const std::string lineEnding = DocumentService::ReadTextFile(absolutePath, &existingText, nullptr)
                                           ? PathUtils::DetectLineEnding(existingText)
                                           : "\n";
        const std::string updatedText = FormatTodoDocument(state, title, description, lineEnding, priority);
        if (!DocumentService::AtomicWriteText(absolutePath, updatedText, &error))
        {
            PopulateResult(result, normalized, 7, false, error);
            return false;
        }

        PopulateResult(result, normalized, 7, false);
        RefreshWorkspaceAfterTodoMutation(workspace);
        return true;
    }

    bool TodoService::DeleteTodo(SlateWorkspaceContext& workspace,
                                 const TodoTicket& ticket,
                                 double elapsedSeconds,
                                 TodoMutationResult* result) const
    {
        (void)elapsedSeconds;
        const auto normalized = PathUtils::NormalizeRelative(ticket.relativePath);
        std::string error;
        if (!workspace.Workspace().DeletePath(normalized, &error))
        {
            PopulateResult(result, normalized, 1, false, error);
            return false;
        }

        if (auto* active = workspace.Documents().Active(); active && PathUtils::NormalizeRelative(active->relativePath) == normalized)
        {
            workspace.Documents().Close();
        }

        PopulateResult(result, normalized, 1, false);
        RefreshWorkspaceAfterTodoMutation(workspace);
        return true;
    }

    bool TodoService::IsTodoDocument(std::string_view text, const fs::path& relativePath)
    {
        TodoTicket ignored;
        return ParseTodoDocument(text, relativePath, &ignored);
    }

    std::string TodoService::FormatTodoDocument(TodoState state,
                                                std::string_view title,
                                                std::string_view description,
                                                std::string_view lineEnding,
                                                TodoPriority priority)
    {
        const std::string eol(lineEnding.empty() ? "\n" : lineEnding);
        const std::string cleanTitle = CleanTitle(title);
        const std::string cleanDescription = CleanDescription(description);

        std::string text;
        text += "---" + eol;
        text += "slate: todo" + eol;
        text += std::string("status: ") + MarkdownService::TodoStateLabel(state) + eol;
        text += std::string("priority: ") + PriorityLabel(priority) + eol;
        text += "---" + eol + eol;
        text += "# " + cleanTitle + eol + eol;
        text += cleanDescription + eol;
        return text;
    }

    bool TodoService::ParseTodoDocument(std::string_view text,
                                        const fs::path& relativePath,
                                        TodoTicket* ticket)
    {
        const auto lines = SplitPreservingLines(text);
        std::string type;
        TodoState state = TodoState::Open;
        TodoPriority priority = TodoPriority::Normal;
        std::size_t frontmatterEnd = 0;
        if (!TryReadFrontmatter(lines, &type, &state, &priority, &frontmatterEnd) || type != "todo")
        {
            return false;
        }

        const std::size_t titleIndex = FindTitleLine(lines, frontmatterEnd + 1);
        std::string title = relativePath.stem().string();
        if (titleIndex < lines.size())
        {
            title = PathUtils::Trim(lines[titleIndex].substr(2));
        }
        if (title.empty())
        {
            title = "Untitled todo";
        }

        std::ostringstream description;
        bool wroteLine = false;
        const std::size_t descriptionStart = titleIndex < lines.size() ? titleIndex + 1 : frontmatterEnd + 1;
        for (std::size_t i = descriptionStart; i < lines.size(); ++i)
        {
            if (!wroteLine && PathUtils::Trim(lines[i]).empty())
            {
                continue;
            }
            if (wroteLine)
            {
                description << '\n';
            }
            description << lines[i];
            wroteLine = true;
        }

        if (ticket)
        {
            ticket->relativePath = PathUtils::NormalizeRelative(relativePath);
            ticket->line = titleIndex < lines.size() ? titleIndex + 1 : 1;
            ticket->endLine = lines.size();
            ticket->title = title;
            ticket->description = CleanDescription(description.str());
            ticket->state = state;
            ticket->priority = priority;
            ticket->done = state == TodoState::Done;
            ticket->tags = TagsFromText(text);
        }
        return true;
    }

    const char* TodoService::PriorityLabel(TodoPriority priority)
    {
        switch (priority)
        {
        case TodoPriority::Low:
            return "Low";
        case TodoPriority::Normal:
            return "Normal";
        case TodoPriority::High:
            return "High";
        case TodoPriority::Urgent:
            return "Urgent";
        }
        return "Normal";
    }

    TodoPriority TodoService::NextPriority(TodoPriority priority)
    {
        switch (priority)
        {
        case TodoPriority::Low:
            return TodoPriority::Normal;
        case TodoPriority::Normal:
            return TodoPriority::High;
        case TodoPriority::High:
            return TodoPriority::Urgent;
        case TodoPriority::Urgent:
            return TodoPriority::Low;
        }
        return TodoPriority::Normal;
    }

    bool TodoService::ParsePriority(std::string_view text, TodoPriority* priority)
    {
        const std::string value = PathUtils::ToLower(PathUtils::Trim(text));
        TodoPriority parsed = TodoPriority::Normal;
        if (value == "low")
        {
            parsed = TodoPriority::Low;
        }
        else if (value == "normal" || value == "medium")
        {
            parsed = TodoPriority::Normal;
        }
        else if (value == "high")
        {
            parsed = TodoPriority::High;
        }
        else if (value == "urgent" || value == "critical")
        {
            parsed = TodoPriority::Urgent;
        }
        else
        {
            return false;
        }

        if (priority)
        {
            *priority = parsed;
        }
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

    int TodoService::PrioritySortKey(TodoPriority priority)
    {
        switch (priority)
        {
        case TodoPriority::Low:
            return 0;
        case TodoPriority::Normal:
            return 1;
        case TodoPriority::High:
            return 2;
        case TodoPriority::Urgent:
            return 3;
        }
        return 1;
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
            ContainsFilter(MarkdownService::TodoStateLabel(todo.state), query) ||
            ContainsFilter(PriorityLabel(todo.priority), query))
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

    fs::path TodoService::TodoFolder()
    {
        return fs::path("Todos");
    }

    fs::path TodoService::MakeTodoPath(const SlateWorkspaceContext& workspace, std::string_view title)
    {
        const std::string slug = Slugify(CleanTitle(title));
        return workspace.Workspace().MakeCollisionSafeMarkdownPath(TodoFolder(), slug + ".md");
    }
}
