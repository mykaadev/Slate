#include "App/Slate/Commands/SlateCommandDispatcher.h"

#include "Core/Runtime/CommandRegistry.h"

#include <utility>

namespace Software::Slate
{
    namespace
    {
        SlateCommandDispatchResult MissingHandler(const std::string& id)
        {
            return SlateCommandDispatchResult::Failure(id, "command handler missing: " + id);
        }
    }

    SlateCommandDispatchResult SlateCommandDispatchResult::Success(std::string id, std::string message)
    {
        SlateCommandDispatchResult result;
        result.handled = true;
        result.succeeded = true;
        result.commandId = std::move(id);
        result.message = std::move(message);
        return result;
    }

    SlateCommandDispatchResult SlateCommandDispatchResult::Failure(std::string id, std::string message)
    {
        SlateCommandDispatchResult result;
        result.handled = true;
        result.succeeded = false;
        result.commandId = std::move(id);
        result.message = std::move(message);
        return result;
    }

    SlateCommandDispatchResult SlateCommandDispatchResult::NotFound(std::string input)
    {
        SlateCommandDispatchResult result;
        result.handled = false;
        result.succeeded = false;
        result.message = std::move(input);
        return result;
    }

    SlateCommandDispatchResult DispatchSlateCommand(const std::string& command,
                                                    Software::Core::Runtime::AppContext& context,
                                                    const SlateCommandHandlers& handlers)
    {
        Software::Core::Runtime::CommandRequest request;
        if (!context.commands.Resolve(command, &request))
        {
            return SlateCommandDispatchResult::NotFound(command);
        }

        const std::string& id = request.id;
        const std::string& arguments = request.arguments;

        if (id == "slate.app.quit")
        {
            if (!handlers.quit)
            {
                return MissingHandler(id);
            }
            handlers.quit();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.document.save")
        {
            if (!handlers.saveDocument)
            {
                return MissingHandler(id);
            }
            return handlers.saveDocument()
                       ? SlateCommandDispatchResult::Success(id)
                       : SlateCommandDispatchResult::Failure(id, "save failed");
        }

        if (id == "slate.app.saveAndQuit")
        {
            if (!handlers.saveDocument || !handlers.quit)
            {
                return MissingHandler(id);
            }
            if (handlers.saveDocument())
            {
                handlers.quit();
                return SlateCommandDispatchResult::Success(id);
            }
            return SlateCommandDispatchResult::Failure(id, "save failed");
        }

        if (id == "slate.journal.openToday")
        {
            if (!handlers.openTodayJournal)
            {
                return MissingHandler(id);
            }
            handlers.openTodayJournal();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.document.open")
        {
            if (arguments.empty())
            {
                return SlateCommandDispatchResult::Failure(id, "open needs a document path");
            }
            if (!handlers.openDocument)
            {
                return MissingHandler(id);
            }
            return handlers.openDocument(arguments)
                       ? SlateCommandDispatchResult::Success(id)
                       : SlateCommandDispatchResult::Failure(id, "could not open document: " + arguments);
        }

        if (id == "slate.document.new")
        {
            if (!handlers.newDocument)
            {
                return MissingHandler(id);
            }
            handlers.newDocument();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.folder.new")
        {
            if (!handlers.newFolder)
            {
                return MissingHandler(id);
            }
            handlers.newFolder();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.workspace.switcher")
        {
            if (!handlers.showWorkspaceSwitcher)
            {
                return MissingHandler(id);
            }
            handlers.showWorkspaceSwitcher();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.todos.show")
        {
            if (!handlers.showTodos)
            {
                return MissingHandler(id);
            }
            handlers.showTodos();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.settings.show")
        {
            if (!handlers.showSettings)
            {
                return MissingHandler(id);
            }
            handlers.showSettings();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.browser.files")
        {
            if (!handlers.showFiles)
            {
                return MissingHandler(id);
            }
            handlers.showFiles();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.search.show")
        {
            if (!handlers.showSearch)
            {
                return MissingHandler(id);
            }
            handlers.showSearch(arguments, arguments.empty());
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.workspace.setup")
        {
            if (!handlers.showWorkspaceSetup)
            {
                return MissingHandler(id);
            }
            handlers.showWorkspaceSetup();
            return SlateCommandDispatchResult::Success(id);
        }

        if (id == "slate.workspace.open")
        {
            if (arguments.empty())
            {
                return SlateCommandDispatchResult::Failure(id, "workspace needs a path");
            }
            if (!handlers.openWorkspace)
            {
                return MissingHandler(id);
            }
            return handlers.openWorkspace(arguments)
                       ? SlateCommandDispatchResult::Success(id)
                       : SlateCommandDispatchResult::Failure(id, "could not open workspace: " + arguments);
        }

        const auto result = context.commands.ExecuteById(id, arguments, context);
        if (!result.handled)
        {
            return SlateCommandDispatchResult::NotFound(command);
        }
        if (!result.succeeded)
        {
            return SlateCommandDispatchResult::Failure(id, result.message.empty() ? ("command failed: " + id) : result.message);
        }
        return SlateCommandDispatchResult::Success(id, result.message);
    }
}
