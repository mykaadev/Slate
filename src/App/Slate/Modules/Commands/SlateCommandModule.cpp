#include "App/Slate/Modules/Commands/SlateCommandModule.h"

#include "Core/Runtime/CommandRegistry.h"

#include <string>
#include <utility>
#include <vector>

namespace Software::Slate::Modules
{
    namespace
    {
        Software::Core::Runtime::CommandDefinition MakeCommand(std::string id,
                                                               std::string label,
                                                               std::string category,
                                                               std::string description,
                                                               std::vector<std::string> aliases,
                                                               bool acceptsArguments = false)
        {
            Software::Core::Runtime::CommandDefinition command;
            command.id = std::move(id);
            command.label = std::move(label);
            command.category = std::move(category);
            command.description = std::move(description);
            command.aliases = std::move(aliases);
            command.acceptsArguments = acceptsArguments;
            return command;
        }
    }

    Software::Core::Runtime::ModuleDescriptor SlateCommandModule::Describe() const
    {
        return {"slate.commands", "Slate Commands", "1.0.0", {"slate.workspace", "slate.editor", "slate.ui", "slate.todos"}};
    }

    void SlateCommandModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        auto& commands = context.commands;
        commands.Register(MakeCommand("slate.app.quit", "Quit", "App", "Save and ask to quit Slate.", {"q", "quit", "exit"}));
        commands.Register(MakeCommand("slate.document.save", "Save Document", "Document", "Save the active document.", {"w", "write", "save"}));
        commands.Register(MakeCommand("slate.app.saveAndQuit", "Save and Quit", "App", "Save the active document and ask to quit.", {"wq", "x"}));
        commands.Register(MakeCommand("slate.journal.openToday", "Open Today's Journal", "Journal", "Open or create today's journal note.", {"today", "j", "journal"}));
        commands.Register(MakeCommand("slate.document.open", "Open Document", "Document", "Open a document by relative path.", {"open"}, true));
        commands.Register(MakeCommand("slate.document.new", "New Note", "Document", "Start the new note flow.", {"new", "note"}, true));
        commands.Register(MakeCommand("slate.folder.new", "New Folder", "Workspace", "Create a folder in the workspace.", {"folder", "mkdir"}));
        commands.Register(MakeCommand("slate.workspace.switcher", "Switch Workspace", "Workspace", "Show the workspace/vault switcher.", {"workspaces", "workspace switcher", "vaults"}));
        commands.Register(MakeCommand("slate.todos.show", "Show Todos", "Todos", "Open the todo overlay.", {"todo", "todos"}));
        commands.Register(MakeCommand("slate.settings.show", "Show Settings", "Settings", "Open app settings.", {"c", "config", "theme", "settings"}));
        commands.Register(MakeCommand("slate.browser.files", "Show Files", "Workspace", "Show the workspace file tree.", {"files", "f", "tree", "library", "l"}));
        commands.Register(MakeCommand("slate.search.show", "Search Workspace", "Search", "Open workspace search.", {"search", "s", "docs"}, true));
        commands.Register(MakeCommand("slate.workspace.setup", "New Workspace", "Workspace", "Open workspace setup.", {"new workspace", "new vault"}));
        commands.Register(MakeCommand("slate.workspace.open", "Open Workspace", "Workspace", "Open a workspace by path.", {"workspace"}, true));
    }
}
