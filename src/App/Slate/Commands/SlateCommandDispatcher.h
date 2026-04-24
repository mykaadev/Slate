#pragma once

#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>

namespace Software::Slate
{
    /** Callback surface used by the shell/modes to bind command ids to current UI actions. */
    struct SlateCommandHandlers
    {
        /** Requests application shutdown. */
        std::function<void()> quit;
        /** Saves the active document. */
        std::function<bool()> saveDocument;
        /** Opens or creates today's journal note. */
        std::function<void()> openTodayJournal;
        /** Opens a workspace-relative document path. */
        std::function<bool(const std::string& path)> openDocument;
        /** Starts the new document flow. */
        std::function<void()> newDocument;
        /** Starts the new folder flow. */
        std::function<void()> newFolder;
        /** Shows the workspace switcher overlay. */
        std::function<void()> showWorkspaceSwitcher;
        /** Shows the todo list overlay. */
        std::function<void()> showTodos;
        /** Activates the settings route. */
        std::function<void()> showSettings;
        /** Activates the file browser route. */
        std::function<void()> showFiles;
        /** Shows the search overlay and optionally seeds its query. */
        std::function<void(const std::string& query, bool clearQuery)> showSearch;
        /** Activates the workspace setup route. */
        std::function<void()> showWorkspaceSetup;
        /** Opens a workspace root path. */
        std::function<bool(const std::string& root)> openWorkspace;
    };

    /** Result returned by the Slate command dispatcher. */
    struct SlateCommandDispatchResult
    {
        /** True when a command id matched a dispatcher branch. */
        bool handled = false;
        /** True when the dispatched command succeeded. */
        bool succeeded = false;
        /** Stable command id that was dispatched. */
        std::string commandId;
        /** Status or error text produced by dispatch. */
        std::string message;

        /** Creates a successful dispatch result. */
        static SlateCommandDispatchResult Success(std::string id, std::string message = {});
        /** Creates a failed dispatch result. */
        static SlateCommandDispatchResult Failure(std::string id, std::string message);
        /** Creates a not-found dispatch result. */
        static SlateCommandDispatchResult NotFound(std::string input);
    };

    /** Dispatches a command-prompt line through stable command ids and shell-provided handlers. */
    SlateCommandDispatchResult DispatchSlateCommand(const std::string& command,
                                                    Software::Core::Runtime::AppContext& context,
                                                    const SlateCommandHandlers& handlers);
}
