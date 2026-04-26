#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    class SlateWorkspaceContext;

    /** Result details returned after a todo document mutation touches workspace content. */
    struct TodoMutationResult
    {
        /** Path of the todo document that was changed. */
        fs::path relativePath;

        /** One-based line that the editor should reveal after the mutation. */
        std::size_t revealLine = 1;

        /** True when the active in-memory todo document was changed instead of a file on disk. */
        bool touchedActiveDocument = false;

        /** Human-readable failure message when the mutation could not be completed. */
        std::string error;
    };

    /** Domain service for todo documents stored as normal Markdown files. */
    class TodoService
    {
    public:
        /** Collects todo documents across the loaded workspace, including unsaved active-document edits. */
        std::vector<TodoTicket> CollectTodos(const SlateWorkspaceContext& workspace) const;

        /** Creates a new todo Markdown document under the workspace todo folder. */
        bool CreateTodo(SlateWorkspaceContext& workspace,
                        TodoState state,
                        std::string_view title,
                        std::string_view description,
                        double elapsedSeconds,
                        TodoMutationResult* result,
                        TodoPriority priority = TodoPriority::Normal) const;

        /** Updates one existing todo Markdown document. */
        bool UpdateTodo(SlateWorkspaceContext& workspace,
                        const TodoTicket& ticket,
                        TodoState state,
                        std::string_view title,
                        std::string_view description,
                        double elapsedSeconds,
                        TodoMutationResult* result,
                        TodoPriority priority = TodoPriority::Normal) const;

        /** Deletes one todo Markdown document. */
        bool DeleteTodo(SlateWorkspaceContext& workspace,
                        const TodoTicket& ticket,
                        double elapsedSeconds,
                        TodoMutationResult* result) const;

        /** Returns true when a Markdown document uses Slate's todo-file format. */
        static bool IsTodoDocument(std::string_view text, const fs::path& relativePath = {});

        /** Builds the Markdown content for a todo document. */
        static std::string FormatTodoDocument(TodoState state,
                                              std::string_view title,
                                              std::string_view description,
                                              std::string_view lineEnding = "\n",
                                              TodoPriority priority = TodoPriority::Normal);

        /** Parses a todo document into a ticket. Returns false for normal notes. */
        static bool ParseTodoDocument(std::string_view text,
                                      const fs::path& relativePath,
                                      TodoTicket* ticket);

        /** Returns the stable display label for a todo priority. */
        static const char* PriorityLabel(TodoPriority priority);

        /** Advances one todo priority. */
        static TodoPriority NextPriority(TodoPriority priority);

        /** Parses a todo priority label from frontmatter. */
        static bool ParsePriority(std::string_view text, TodoPriority* priority);

        /** Stable state ordering used by todo views. */
        static int StateSortKey(TodoState state);

        /** Stable priority ordering used by todo views. */
        static int PrioritySortKey(TodoPriority priority);

        /** Returns whether a todo matches a free-text query. */
        static bool MatchesQuery(const TodoTicket& todo, const std::string& query);

    private:
        /** Returns the workspace-relative folder that owns todo documents. */
        static fs::path TodoFolder();

        /** Builds a collision-safe relative path for a new todo document. */
        static fs::path MakeTodoPath(const SlateWorkspaceContext& workspace, std::string_view title);
    };
}
