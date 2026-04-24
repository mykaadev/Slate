#pragma once

#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>

namespace Software::Slate
{
    /** Callback surface used by the shell/modes to bind command ids to current UI actions. */
    struct SlateCommandHandlers
    {
        std::function<void()> quit;
        std::function<bool()> saveDocument;
        std::function<void()> openTodayJournal;
        std::function<bool(const std::string& path)> openDocument;
        std::function<void()> newDocument;
        std::function<void()> newFolder;
        std::function<void()> showWorkspaceSwitcher;
        std::function<void()> showTodos;
        std::function<void()> showSettings;
        std::function<void()> showFiles;
        std::function<void(const std::string& query, bool clearQuery)> showSearch;
        std::function<void()> showWorkspaceSetup;
        std::function<bool(const std::string& root)> openWorkspace;
    };

    /** Result returned by the Slate command dispatcher. */
    struct SlateCommandDispatchResult
    {
        bool handled = false;
        bool succeeded = false;
        std::string commandId;
        std::string message;

        static SlateCommandDispatchResult Success(std::string id, std::string message = {});
        static SlateCommandDispatchResult Failure(std::string id, std::string message);
        static SlateCommandDispatchResult NotFound(std::string input);
    };

    /** Dispatches a command-prompt line through stable command ids and shell-provided handlers. */
    SlateCommandDispatchResult DispatchSlateCommand(const std::string& command,
                                                    Software::Core::Runtime::AppContext& context,
                                                    const SlateCommandHandlers& handlers);
}
