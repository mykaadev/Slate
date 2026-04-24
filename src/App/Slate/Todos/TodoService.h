#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <string>
#include <vector>

namespace Software::Slate
{
    class SlateWorkspaceContext;

    /** Domain service for collecting and querying markdown todo tickets. */
    class TodoService
    {
    public:
        /** Collects todos across the loaded workspace, including unsaved active-document edits. */
        std::vector<TodoTicket> CollectTodos(const SlateWorkspaceContext& workspace) const;

        /** Stable state ordering used by todo views. */
        static int StateSortKey(TodoState state);

        /** Returns whether a todo matches a free-text query. */
        static bool MatchesQuery(const TodoTicket& todo, const std::string& query);
    };
}
