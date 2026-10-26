#include "App/Slate/Modules/Plugins/SlatePluginModule.h"

#include "App/Slate/Core/AppPaths.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Extensions/SlateShellBridge.h"
#include "App/Slate/Plugins/PluginManager.h"
#include "App/Slate/State/SlateUiState.h"
#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/CommandRegistry.h"
#include "Core/Runtime/ServiceLocator.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

#include <utility>

namespace Software::Slate::Modules
{
    namespace
    {
        using Software::Core::Runtime::AppContext;
        using Software::Core::Runtime::CommandDefinition;
        using Software::Core::Runtime::CommandRequest;
        using Software::Core::Runtime::CommandResult;

        CommandDefinition MakeCommand(std::string id,
                                      std::string label,
                                      std::string description,
                                      std::vector<std::string> aliases,
                                      bool acceptsArguments,
                                      std::function<CommandResult(AppContext&, const CommandRequest&)> execute)
        {
            CommandDefinition command;
            command.id = std::move(id);
            command.label = std::move(label);
            command.category = "Plugins";
            command.description = std::move(description);
            command.aliases = std::move(aliases);
            command.acceptsArguments = acceptsArguments;
            command.execute = std::move(execute);
            return command;
        }

        void RevealInExplorer(const fs::path& path)
        {
            std::error_code ec;
            fs::create_directories(path, ec);
#if defined(_WIN32)
            ::ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
        }

        const char* InitTemplate()
        {
            return R"lua(-- Slate runs this file on startup, like an always-on plugin.
-- Every slate.* API from docs/PLUGINS.md works here. Save to reload.

-- slate.config.set("font_size", 16)
-- slate.config.set("live_preview", true)
-- slate.keymap.set("ctrl+shift+j", "today")

-- slate.slash.register { id = "sig", description = "Insert my signature", insert = "-- me" }
)lua";
        }
    }

    Software::Core::Runtime::ModuleDescriptor SlatePluginModule::Describe() const
    {
        return {"slate.plugins", "Slate Lua Plugins", "1.0.0", {"slate.workspace", "slate.editor", "slate.ui", "slate.startup"}};
    }

    void SlatePluginModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        m_plugins = std::make_shared<Software::Slate::PluginManager>();
        context.services.Register<Software::Slate::PluginManager>(m_plugins);

        auto plugins = m_plugins;
        context.commands.Register(MakeCommand(
            "slate.plugins.show", "Manage Plugins", "Open the plugin list in settings.", {"plugins"}, false,
            [](AppContext& app, const CommandRequest& request) {
                if (const auto ui = app.services.Resolve<Software::Slate::SlateUiState>())
                {
                    ui->settingsJumpToPlugins = true;
                }
                if (const auto bridge = app.services.Resolve<Software::Slate::SlateShellBridge>())
                {
                    bridge->RunCommand("slate.settings.show");
                }
                return CommandResult::Handled(request.id);
            }));

        context.commands.Register(MakeCommand(
            "slate.plugins.reload", "Reload Plugins", "Rediscover plugin folders and reload every plugin, or one by id.",
            {"reload", "plugins reload"}, true,
            [plugins](AppContext&, const CommandRequest& request) {
                if (request.arguments.empty())
                {
                    plugins->ReloadAll();
                    return CommandResult::Handled(request.id, "plugins reloaded");
                }
                std::string error;
                return plugins->Reload(request.arguments, &error)
                           ? CommandResult::Handled(request.id, "reloaded " + request.arguments)
                           : CommandResult::Failed(request.id, error);
            }));

        context.commands.Register(MakeCommand(
            "slate.plugins.new", "New Plugin", "Create a starter plugin in your plugin folder.", {"plugin new", "new plugin"}, true,
            [plugins](AppContext&, const CommandRequest& request) {
                if (request.arguments.empty())
                {
                    return CommandResult::Failed(request.id, "usage: plugin new <name>");
                }
                fs::path created;
                std::string error;
                if (!plugins->CreatePlugin(request.arguments, &created, &error))
                {
                    return CommandResult::Failed(request.id, error);
                }
                RevealInExplorer(created);
                return CommandResult::Handled(request.id, "created " + created.u8string());
            }));

        context.commands.Register(MakeCommand(
            "slate.plugins.folder", "Open Plugin Folder", "Open your plugin folder in the file explorer.", {"plugin folder"}, false,
            [](AppContext&, const CommandRequest& request) {
                RevealInExplorer(Software::Slate::PluginManager::UserPluginDirectory());
                return CommandResult::Handled(request.id);
            }));

        context.commands.Register(MakeCommand(
            "slate.plugins.enable", "Enable Plugin", "Enable a plugin by id.", {"plugin enable"}, true,
            [plugins](AppContext&, const CommandRequest& request) {
                std::string error;
                return plugins->SetEnabled(request.arguments, true, &error)
                           ? CommandResult::Handled(request.id, "enabled " + request.arguments)
                           : CommandResult::Failed(request.id, error);
            }));

        context.commands.Register(MakeCommand(
            "slate.plugins.disable", "Disable Plugin", "Disable a plugin by id.", {"plugin disable"}, true,
            [plugins](AppContext&, const CommandRequest& request) {
                std::string error;
                return plugins->SetEnabled(request.arguments, false, &error)
                           ? CommandResult::Handled(request.id, "disabled " + request.arguments)
                           : CommandResult::Failed(request.id, error);
            }));

        context.commands.Register(MakeCommand(
            "slate.config.init", "Edit init.lua", "Create your personal init.lua and open its folder.", {"init"}, false,
            [plugins](AppContext&, const CommandRequest& request) {
                const fs::path path = Software::Slate::AppPaths::UserInitScript();
                if (!fs::exists(path))
                {
                    std::error_code ec;
                    fs::create_directories(path.parent_path(), ec);
                    std::string error;
                    if (!Software::Slate::DocumentService::AtomicWriteText(path, InitTemplate(), &error))
                    {
                        return CommandResult::Failed(request.id, error);
                    }
                    plugins->ReloadAll();
                }
#if defined(_WIN32)
                ::ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
                return CommandResult::Handled(request.id, path.u8string());
            }));
    }

    void SlatePluginModule::Start(Software::Core::Runtime::ModuleContext& context)
    {
        m_plugins->Start(context.app);
    }

    void SlatePluginModule::Update(Software::Core::Runtime::ModuleContext& context)
    {
        m_plugins->Update(context.app);
    }

    void SlatePluginModule::Stop(Software::Core::Runtime::ModuleContext&)
    {
        m_plugins->Stop();
    }
}
