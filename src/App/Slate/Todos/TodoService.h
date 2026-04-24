#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Software::Slate
{
    class SlateWorkspaceContext;

    /** Result details returned after a todo edit touches workspace content. */
    struct TodoMutationResult
    {
        /** Path of the document that was changed. */
        fs::path relativePath;

        /** One-based line that the editor should reveal after the mutation. */
        std::size_t revealLine = 1;

        /** True when the active in-memory document was changed instead of a file on disk. */
        bool touchedActiveDocument = false;

        /** Human-readable failure message when the mutation could not be completed. */
        std::string error;
    };

    /** Domain service for collecting, querying, updating, and deleting markdown todo tickets. */
    class TodoService
    {
    public:
        /** Collects todos across the loaded workspace, including unsaved active-document edits. */
        std::vector<TodoTicket> CollectTodos(const SlateWorkspaceContext& workspace) const;

        /** Updates one existing todo block in either the active document or its backing file. */
        bool UpdateTodo(SlateWorkspaceContext& workspace,
                        const TodoTicket& ticket,
                        TodoState state,
                        std::string_view title,
                        std::string_view description,
                        double elapsedSeconds,
                        TodoMutationResult* result) const;

        /** Deletes one existing todo block in either the active document or its backing file. */
        bool DeleteTodo(SlateWorkspaceContext& workspace,
                        const TodoTicket& ticket,
                        double elapsedSeconds,
                        TodoMutationResult* result) const;

        /** Stable state ordering used by todo views. */
        static int StateSortKey(TodoState state);

        /** Returns whether a todo matches a free-text query. */
        static bool MatchesQuery(const TodoTicket& todo, const std::string& query);
    };
}
