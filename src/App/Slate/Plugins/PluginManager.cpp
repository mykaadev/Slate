#include "App/Slate/Plugins/PluginManager.h"

#include "App/Slate/Core/AppPaths.h"
#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Core/SettingsFile.h"
#include "App/Slate/Core/SlateEvents.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/Extensions/SlateShellBridge.h"
#include "App/Slate/Plugins/PluginInstance.h"
#include "App/Slate/State/SlateWorkspaceContext.h"

#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/CommandRegistry.h"

#include "imgui.h"
#include <nlohmann/json.hpp>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        constexpr double CallBudgetSeconds = 2.0;
        constexpr std::size_t MaxLogLines = 200;
        constexpr const char* UserInitId = "user-init";

        std::string Slugify(const std::string& value)
        {
            std::string slug;
            for (const unsigned char ch : value)
            {
                if (std::isalnum(ch) != 0)
                {
                    slug.push_back(static_cast<char>(std::tolower(ch)));
                }
                else if (!slug.empty() && slug.back() != '-')
                {
                    slug.push_back('-');
                }
            }
            while (!slug.empty() && slug.back() == '-')
            {
                slug.pop_back();
            }
            return slug;
        }

        bool ReadManifest(const fs::path& directory, PluginManifest* manifest)
        {
            manifest->id = Slugify(directory.filename().u8string());
            manifest->name = directory.filename().u8string();

            const fs::path manifestPath = directory / "plugin.json";
            if (!fs::exists(manifestPath))
            {
                return fs::exists(directory / manifest->main);
            }

            std::string text;
            if (!DocumentService::ReadTextFile(manifestPath, &text))
            {
                return false;
            }

            const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
            if (json.is_discarded() || !json.is_object())
            {
                return false;
            }

            const auto readString = [&json](const char* key, std::string* out) {
                const auto it = json.find(key);
                if (it != json.end() && it->is_string())
                {
                    *out = it->get<std::string>();
                }
            };
            readString("id", &manifest->id);
            readString("name", &manifest->name);
            readString("version", &manifest->version);
            readString("description", &manifest->description);
            readString("author", &manifest->author);
            readString("main", &manifest->main);
            if (const auto it = json.find("api"); it != json.end() && it->is_number_integer())
            {
                manifest->apiVersion = it->get<int>();
            }

            manifest->id = Slugify(manifest->id);
            return !manifest->id.empty();
        }

        const char* PluginTemplate()
        {
            return R"lua(-- A Slate plugin. Edit and save: Slate reloads it automatically.
-- API reference: docs/PLUGINS.md

slate.command.register {
  id = "hello",
  label = "Say hello",
  description = "Shows a greeting in the status line",
  aliases = { "hello" },
  run = function(args)
    slate.notify("hello from " .. slate.plugin.name .. (args ~= "" and (": " .. args) or ""))
  end,
}

slate.slash.register {
  id = "now",
  label = "now",
  description = "Insert the current time",
  run = function()
    return os.date("%H:%M")
  end,
}

slate.on("document_open", function(path)
  slate.log("opened " .. path)
end)
)lua";
        }
    }

    PluginManager::PluginManager() = default;

    PluginManager::~PluginManager()
    {
        UnloadAll();
    }

    void PluginManager::Start(Software::Core::Runtime::AppContext& context)
    {
        m_app = &context;
        LoadEnabledState();

        context.events.Subscribe<Events::DocumentOpened>([this](const Events::DocumentOpened& event) {
            const std::string path = event.relativePath.generic_u8string();
            Emit("document_open", [path](PluginInstance& plugin) {
                lua_pushlstring(plugin.L, path.data(), path.size());
                return 1;
            });
        });
        context.events.Subscribe<Events::DocumentSaved>([this](const Events::DocumentSaved& event) {
            const std::string path = event.relativePath.generic_u8string();
            Emit("document_save", [path](PluginInstance& plugin) {
                lua_pushlstring(plugin.L, path.data(), path.size());
                return 1;
            });
        });
        context.events.Subscribe<Events::DocumentWillSave>([this](const Events::DocumentWillSave& event) {
            EmitBeforeSave(event.relativePath, event.text);
        });

        m_started = true;
        LoadAll();
        Emit("ready", [](PluginInstance&) { return 0; });
    }

    void PluginManager::Update(Software::Core::Runtime::AppContext& context)
    {
        if (!m_started)
        {
            return;
        }
        m_app = &context;

        // Workspace plugins follow the open workspace, however it was opened
        if (const auto workspace = context.services.Resolve<SlateWorkspaceContext>())
        {
            const fs::path root = workspace->HasWorkspaceLoaded() ? workspace->Workspace().Root() : fs::path();
            if (root != m_loadedWorkspaceRoot)
            {
                ReloadAll();
                const std::string rootText = root.u8string();
                Emit("workspace_open", [rootText](PluginInstance& plugin) {
                    lua_pushlstring(plugin.L, rootText.data(), rootText.size());
                    return 1;
                });
            }
        }

        const double now = context.frame.elapsedSeconds;
        RunTimers(now);
        CheckDocumentChange(now);
        CheckKeyBindings();
        CheckHotReload(now);
        if (m_themesDirty)
        {
            SyncThemeRoots();
        }
    }

    void PluginManager::Stop()
    {
        Emit("quit", [](PluginInstance&) { return 0; });
        UnloadAll();
        m_started = false;
    }

    std::vector<PluginInfo> PluginManager::Plugins() const
    {
        std::vector<PluginInfo> plugins;
        plugins.reserve(m_plugins.size());
        for (const auto& plugin : m_plugins)
        {
            plugins.push_back(plugin->info);
        }
        return plugins;
    }

    bool PluginManager::SetEnabled(const std::string& id, bool enabled, std::string* error)
    {
        auto it = std::find_if(m_plugins.begin(), m_plugins.end(), [&](const auto& plugin) {
            return plugin->info.manifest.id == id;
        });
        if (it == m_plugins.end())
        {
            if (error)
            {
                *error = "unknown plugin: " + id;
            }
            return false;
        }

        PluginInstance& plugin = **it;
        m_enabledOverrides[id] = enabled;
        SaveEnabledState();
        plugin.info.enabled = enabled;
        if (enabled)
        {
            const bool loaded = LoadPlugin(plugin);
            if (!loaded && error)
            {
                *error = plugin.info.error;
            }
            if (loaded)
            {
                Emit("ready", [&plugin](PluginInstance& target) { return &target == &plugin ? 0 : -1; });
            }
            MarkThemesDirty();
            return loaded;
        }

        UnloadPlugin(plugin);
        plugin.info.status = PluginStatus::Disabled;
        MarkThemesDirty();
        return true;
    }

    bool PluginManager::Reload(const std::string& id, std::string* error)
    {
        PluginInstance* plugin = nullptr;
        for (const auto& candidate : m_plugins)
        {
            if (candidate->info.manifest.id == id)
            {
                plugin = candidate.get();
            }
        }
        if (!plugin)
        {
            if (error)
            {
                *error = "unknown plugin: " + id;
            }
            return false;
        }
        if (!plugin->info.enabled)
        {
            if (error)
            {
                *error = "plugin is disabled: " + id;
            }
            return false;
        }

        const bool loaded = LoadPlugin(*plugin);
        if (!loaded && error)
        {
            *error = plugin->info.error;
        }
        if (loaded)
        {
            Emit("ready", [plugin](PluginInstance& target) { return &target == plugin ? 0 : -1; });
        }
        return loaded;
    }

    void PluginManager::ReloadAll()
    {
        UnloadAll();
        LoadAll();
    }

    bool PluginManager::CreatePlugin(const std::string& name, fs::path* createdDirectory, std::string* error)
    {
        const std::string id = Slugify(name);
        if (id.empty())
        {
            if (error)
            {
                *error = "plugin name needs letters or digits";
            }
            return false;
        }

        const fs::path directory = UserPluginDirectory() / fs::u8path(id);
        if (fs::exists(directory))
        {
            if (error)
            {
                *error = "plugin already exists: " + directory.u8string();
            }
            return false;
        }

        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec)
        {
            if (error)
            {
                *error = ec.message();
            }
            return false;
        }

        nlohmann::json manifest;
        manifest["id"] = id;
        manifest["name"] = name;
        manifest["version"] = "0.1.0";
        manifest["description"] = "Describe what " + name + " does";
        manifest["main"] = "main.lua";
        manifest["api"] = ApiVersion;
        if (!DocumentService::AtomicWriteText(directory / "plugin.json", manifest.dump(2) + "\n", error) ||
            !DocumentService::AtomicWriteText(directory / "main.lua", PluginTemplate(), error))
        {
            return false;
        }

        if (createdDirectory)
        {
            *createdDirectory = directory;
        }
        ReloadAll();
        return true;
    }

    fs::path PluginManager::BundledPluginDirectory()
    {
        return AppPaths::ExecutableDirectory() / "assets" / "slate" / "plugins";
    }

    fs::path PluginManager::UserPluginDirectory()
    {
        return AppPaths::UserPluginDirectory();
    }

    fs::path PluginManager::WorkspacePluginDirectory() const
    {
        if (!m_app)
        {
            return {};
        }
        const auto workspace = m_app->services.Resolve<SlateWorkspaceContext>();
        if (!workspace || !workspace->HasWorkspaceLoaded())
        {
            return {};
        }
        return workspace->Workspace().Root() / ".slate" / "plugins";
    }

    Software::Core::Runtime::AppContext* PluginManager::App() const
    {
        return m_app;
    }

    void PluginManager::Log(PluginInstance& plugin, const std::string& message, bool isError)
    {
        plugin.info.log.push_back((isError ? "error: " : "") + message);
        if (plugin.info.log.size() > MaxLogLines)
        {
            plugin.info.log.erase(plugin.info.log.begin(),
                                  plugin.info.log.begin() + static_cast<std::ptrdiff_t>(plugin.info.log.size() - MaxLogLines));
        }
        if (isError)
        {
            plugin.info.error = message;
            if (m_app)
            {
                if (const auto bridge = m_app->services.Resolve<SlateShellBridge>())
                {
                    bridge->Notify(plugin.info.manifest.id + ": " + message, true);
                }
            }
        }
    }

    bool PluginManager::Call(PluginInstance& plugin, int argumentCount, int resultCount, std::string* error)
    {
        lua_State* L = plugin.L;
        const int functionIndex = lua_gettop(L) - argumentCount;
        lua_pushcfunction(L, [](lua_State* state) -> int {
            const char* message = lua_tostring(state, 1);
            luaL_traceback(state, state, message ? message : "plugin error", 1);
            return 1;
        });
        lua_insert(L, functionIndex);

        // Nested calls keep the outer deadline so a callback cannot extend its own budget
        const bool outermost = plugin.callDeadline <= 0.0;
        if (outermost)
        {
            plugin.callDeadline = PluginLuaApi::SteadySeconds() + CallBudgetSeconds;
        }
        const int status = lua_pcall(L, argumentCount, resultCount, functionIndex);
        if (outermost)
        {
            plugin.callDeadline = 0.0;
        }
        lua_remove(L, functionIndex);

        if (status != LUA_OK)
        {
            std::string message = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown plugin error";
            lua_pop(L, 1);
            Log(plugin, message, true);
            if (error)
            {
                *error = message;
            }
            return false;
        }
        return true;
    }

    bool PluginManager::CallRef(const std::string& pluginId,
                                std::uint64_t generation,
                                int ref,
                                const std::function<int(PluginInstance&)>& pushArguments,
                                int resultCount,
                                const std::function<void(PluginInstance&)>& readResults)
    {
        PluginInstance* plugin = FindLoaded(pluginId);
        if (!plugin || plugin->generation != generation || ref == LUA_NOREF || ref == LUA_REFNIL)
        {
            return false;
        }

        lua_State* L = plugin->L;
        const int top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        const int argumentCount = pushArguments ? pushArguments(*plugin) : 0;
        if (argumentCount < 0)
        {
            lua_settop(L, top);
            return false;
        }
        if (!Call(*plugin, argumentCount, resultCount))
        {
            lua_settop(L, top);
            return false;
        }
        if (readResults)
        {
            readResults(*plugin);
        }
        lua_settop(L, top);
        return true;
    }

    PluginInstance* PluginManager::FindLoaded(const std::string& id)
    {
        for (const auto& plugin : m_plugins)
        {
            if (plugin->info.manifest.id == id && plugin->L)
            {
                return plugin.get();
            }
        }
        return nullptr;
    }

    int PluginManager::AddTimer(PluginInstance& plugin, double delaySeconds, bool repeat, int ref)
    {
        PluginTimer timer;
        timer.id = plugin.nextTimerId++;
        timer.interval = std::max(0.016, delaySeconds);
        timer.due = (m_app ? m_app->frame.elapsedSeconds : 0.0) + timer.interval;
        timer.repeat = repeat;
        timer.ref = ref;
        plugin.timers.push_back(timer);
        return timer.id;
    }

    void PluginManager::CancelTimer(PluginInstance& plugin, int timerId)
    {
        const auto it = std::find_if(plugin.timers.begin(), plugin.timers.end(), [&](const PluginTimer& timer) {
            return timer.id == timerId;
        });
        if (it != plugin.timers.end())
        {
            luaL_unref(plugin.L, LUA_REGISTRYINDEX, it->ref);
            plugin.timers.erase(it);
        }
    }

    void PluginManager::MarkThemesDirty()
    {
        m_themesDirty = true;
    }

    std::vector<PluginManager::Candidate> PluginManager::Discover() const
    {
        std::vector<Candidate> candidates;
        const auto scan = [&candidates](const fs::path& root, PluginSource source) {
            std::error_code ec;
            if (root.empty() || !fs::is_directory(root, ec))
            {
                return;
            }

            std::vector<fs::path> directories;
            for (const auto& entry : fs::directory_iterator(root, ec))
            {
                if (entry.is_directory(ec))
                {
                    directories.push_back(entry.path());
                }
            }
            std::sort(directories.begin(), directories.end());

            for (const auto& directory : directories)
            {
                Candidate candidate;
                candidate.directory = directory;
                candidate.source = source;
                if (!ReadManifest(directory, &candidate.manifest))
                {
                    continue;
                }

                // Later sources replace earlier ones so users can override bundled plugins
                auto existing = std::find_if(candidates.begin(), candidates.end(), [&](const Candidate& other) {
                    return other.manifest.id == candidate.manifest.id;
                });
                if (existing != candidates.end())
                {
                    *existing = std::move(candidate);
                }
                else
                {
                    candidates.push_back(std::move(candidate));
                }
            }
        };

        scan(BundledPluginDirectory(), PluginSource::Bundled);
        const fs::path workingCopy = fs::current_path() / "assets" / "slate" / "plugins";
        if (workingCopy.lexically_normal() != BundledPluginDirectory().lexically_normal())
        {
            scan(workingCopy, PluginSource::Bundled);
        }
        scan(UserPluginDirectory(), PluginSource::User);
        scan(WorkspacePluginDirectory(), PluginSource::Workspace);

        const fs::path initScript = AppPaths::UserInitScript();
        if (fs::exists(initScript))
        {
            Candidate init;
            init.directory = initScript.parent_path();
            init.source = PluginSource::UserInit;
            init.manifest.id = UserInitId;
            init.manifest.name = "init.lua";
            init.manifest.description = "Your personal startup script";
            init.manifest.main = initScript.filename().u8string();
            candidates.push_back(std::move(init));
        }
        return candidates;
    }

    void PluginManager::LoadAll()
    {
        m_loadedWorkspaceRoot.clear();
        if (m_app)
        {
            if (const auto workspace = m_app->services.Resolve<SlateWorkspaceContext>(); workspace && workspace->HasWorkspaceLoaded())
            {
                m_loadedWorkspaceRoot = workspace->Workspace().Root();
            }
        }

        for (auto& candidate : Discover())
        {
            auto plugin = std::make_unique<PluginInstance>();
            plugin->manager = this;
            plugin->info.manifest = std::move(candidate.manifest);
            plugin->info.directory = std::move(candidate.directory);
            plugin->info.source = candidate.source;

            // Workspace plugins come from folders the user may not have written, so they start disabled
            const bool defaultEnabled = plugin->info.source != PluginSource::Workspace;
            const auto overrideIt = m_enabledOverrides.find(plugin->info.manifest.id);
            plugin->info.enabled = overrideIt == m_enabledOverrides.end() ? defaultEnabled : overrideIt->second;

            PluginInstance& loaded = *plugin;
            m_plugins.push_back(std::move(plugin));
            if (loaded.info.enabled)
            {
                LoadPlugin(loaded);
            }
        }
        MarkThemesDirty();
    }

    void PluginManager::UnloadAll()
    {
        for (auto& plugin : m_plugins)
        {
            UnloadPlugin(*plugin);
        }
        m_plugins.clear();
    }

    bool PluginManager::LoadPlugin(PluginInstance& plugin)
    {
        UnloadPlugin(plugin);
        plugin.info.error.clear();
        plugin.info.commands.clear();
        plugin.generation = m_nextGeneration++;
        plugin.scriptStamp = ScriptStamp(plugin);

        if (plugin.info.manifest.apiVersion > ApiVersion)
        {
            plugin.info.status = PluginStatus::Failed;
            plugin.info.error = "needs plugin api " + std::to_string(plugin.info.manifest.apiVersion) +
                                ", this Slate has " + std::to_string(ApiVersion);
            return false;
        }

        const fs::path mainPath = plugin.info.directory / fs::u8path(plugin.info.manifest.main);
        std::string source;
        std::string readError;
        if (!DocumentService::ReadTextFile(mainPath, &source, &readError))
        {
            plugin.info.status = PluginStatus::Failed;
            plugin.info.error = "could not read " + mainPath.u8string();
            return false;
        }
        if (source.rfind("\xEF\xBB\xBF", 0) == 0)
        {
            source.erase(0, 3);
        }

        plugin.L = luaL_newstate();
        if (!plugin.L)
        {
            plugin.info.status = PluginStatus::Failed;
            plugin.info.error = "could not create a Lua state";
            return false;
        }

        PluginLuaApi::Install(plugin);
        const std::string chunkName = "@" + plugin.info.manifest.main;
        if (luaL_loadbufferx(plugin.L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK)
        {
            const std::string message = lua_tostring(plugin.L, -1) ? lua_tostring(plugin.L, -1) : "syntax error";
            Log(plugin, message, true);
            UnloadPlugin(plugin);
            plugin.info.status = PluginStatus::Failed;
            plugin.info.error = message;
            return false;
        }

        std::string error;
        if (!Call(plugin, 0, 0, &error))
        {
            UnloadPlugin(plugin);
            plugin.info.status = PluginStatus::Failed;
            plugin.info.error = error;
            return false;
        }

        plugin.info.status = PluginStatus::Loaded;
        Log(plugin, "loaded " + plugin.info.manifest.name + " " + plugin.info.manifest.version);
        return true;
    }

    void PluginManager::UnloadPlugin(PluginInstance& plugin)
    {
        if (!plugin.L)
        {
            return;
        }

        if (const auto it = plugin.events.find("unload"); it != plugin.events.end())
        {
            for (const int ref : it->second)
            {
                lua_rawgeti(plugin.L, LUA_REGISTRYINDEX, ref);
                Call(plugin, 0, 0);
            }
        }

        PluginLuaApi::RemoveContributions(plugin);
        plugin.events.clear();
        plugin.timers.clear();
        lua_close(plugin.L);
        plugin.L = nullptr;
        if (plugin.info.status == PluginStatus::Loaded)
        {
            plugin.info.status = PluginStatus::Disabled;
        }
    }

    void PluginManager::Emit(const std::string& eventName, const std::function<int(PluginInstance&)>& pushArguments)
    {
        // Index loop: a listener may enable or reload plugins while we iterate
        for (std::size_t index = 0; index < m_plugins.size(); ++index)
        {
            PluginInstance* plugin = m_plugins[index].get();
            if (!plugin->L)
            {
                continue;
            }
            const auto it = plugin->events.find(eventName);
            if (it == plugin->events.end())
            {
                continue;
            }

            const std::vector<int> listeners = it->second;
            const std::string id = plugin->info.manifest.id;
            const std::uint64_t generation = plugin->generation;
            for (const int ref : listeners)
            {
                if (!CallRef(id, generation, ref, pushArguments, 0))
                {
                    if (!FindLoaded(id))
                    {
                        break;
                    }
                }
            }
        }
    }

    void PluginManager::EmitBeforeSave(const fs::path& relativePath, std::string* text)
    {
        if (!text)
        {
            return;
        }

        const std::string path = relativePath.generic_u8string();
        for (std::size_t index = 0; index < m_plugins.size(); ++index)
        {
            PluginInstance* plugin = m_plugins[index].get();
            if (!plugin->L)
            {
                continue;
            }
            const auto it = plugin->events.find("document_before_save");
            if (it == plugin->events.end())
            {
                continue;
            }

            const std::vector<int> listeners = it->second;
            for (const int ref : listeners)
            {
                CallRef(plugin->info.manifest.id, plugin->generation, ref,
                        [&](PluginInstance& target) {
                            lua_pushlstring(target.L, path.data(), path.size());
                            lua_pushlstring(target.L, text->data(), text->size());
                            return 2;
                        },
                        1,
                        [&](PluginInstance& target) {
                            if (lua_type(target.L, -1) == LUA_TSTRING)
                            {
                                std::size_t length = 0;
                                const char* value = lua_tolstring(target.L, -1, &length);
                                text->assign(value, length);
                            }
                        });
            }
        }
    }

    void PluginManager::RunTimers(double now)
    {
        for (std::size_t index = 0; index < m_plugins.size(); ++index)
        {
            PluginInstance* plugin = m_plugins[index].get();
            if (!plugin->L || plugin->timers.empty())
            {
                continue;
            }

            std::vector<PluginTimer> due;
            for (auto& timer : plugin->timers)
            {
                if (timer.due <= now)
                {
                    due.push_back(timer);
                    timer.due = now + timer.interval;
                }
            }

            const std::string id = plugin->info.manifest.id;
            const std::uint64_t generation = plugin->generation;
            for (const auto& timer : due)
            {
                CallRef(id, generation, timer.ref, {}, 0);
                PluginInstance* live = FindLoaded(id);
                if (!live || live->generation != generation)
                {
                    break;
                }
                if (!timer.repeat)
                {
                    CancelTimer(*live, timer.id);
                }
            }
        }
    }

    void PluginManager::CheckDocumentChange(double now)
    {
        const auto workspace = m_app->services.Resolve<SlateWorkspaceContext>();
        const auto* document = workspace ? workspace->Documents().Active() : nullptr;
        if (!document)
        {
            m_lastSeenEditSeconds = -1.0;
            m_documentChangePending = false;
            return;
        }

        if (document->relativePath != m_lastSeenEditPath)
        {
            m_lastSeenEditPath = document->relativePath;
            m_lastSeenEditSeconds = document->lastEditSeconds;
            m_documentChangePending = false;
            return;
        }

        if (document->lastEditSeconds != m_lastSeenEditSeconds)
        {
            m_lastSeenEditSeconds = document->lastEditSeconds;
            m_documentChangePending = true;
        }

        // Debounced so plugins see one change event per typing burst
        if (m_documentChangePending && now - m_lastSeenEditSeconds > 0.3)
        {
            m_documentChangePending = false;
            const std::string path = document->relativePath.generic_u8string();
            Emit("document_change", [path](PluginInstance& plugin) {
                lua_pushlstring(plugin.L, path.data(), path.size());
                return 1;
            });
        }
    }

    void PluginManager::CheckKeyBindings()
    {
        const auto registry = m_app->services.Resolve<SlateExtensionRegistry>();
        const auto bridge = m_app->services.Resolve<SlateShellBridge>();
        if (!registry || !bridge || registry->KeyBindings().empty() || ImGui::GetCurrentContext() == nullptr)
        {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput)
        {
            return;
        }

        for (const auto& binding : registry->KeyBindings())
        {
            if (binding.binding.key != ImGuiKey_None &&
                ImGui::IsKeyPressed(binding.binding.key, false) &&
                io.KeyCtrl == binding.binding.ctrl &&
                io.KeyShift == binding.binding.shift &&
                io.KeyAlt == binding.binding.alt)
            {
                bridge->RunCommand(binding.command);
                break;
            }
        }
    }

    void PluginManager::CheckHotReload(double now)
    {
        if (now - m_lastReloadCheck < 1.0)
        {
            return;
        }
        m_lastReloadCheck = now;

        for (std::size_t index = 0; index < m_plugins.size(); ++index)
        {
            PluginInstance& plugin = *m_plugins[index];
            if (!plugin.info.enabled)
            {
                continue;
            }

            const std::uint64_t stamp = ScriptStamp(plugin);
            if (stamp != 0 && stamp != plugin.scriptStamp)
            {
                const bool loaded = LoadPlugin(plugin);
                if (const auto bridge = m_app->services.Resolve<SlateShellBridge>())
                {
                    if (loaded)
                    {
                        bridge->Notify("reloaded plugin " + plugin.info.manifest.id);
                    }
                }
                if (loaded)
                {
                    PluginInstance* reloaded = &plugin;
                    Emit("ready", [reloaded](PluginInstance& target) { return &target == reloaded ? 0 : -1; });
                }
                MarkThemesDirty();
            }
        }
    }

    void PluginManager::SyncThemeRoots()
    {
        m_themesDirty = false;
        const auto workspace = m_app ? m_app->services.Resolve<SlateWorkspaceContext>() : nullptr;
        if (!workspace)
        {
            return;
        }

        std::vector<fs::path> roots;
        for (const auto& plugin : m_plugins)
        {
            std::error_code ec;
            const fs::path themes = plugin->info.directory / "themes";
            if (plugin->info.enabled && plugin->info.source != PluginSource::UserInit && fs::is_directory(themes, ec))
            {
                roots.push_back(themes);
            }
        }
        workspace->Theme().SetExtraPresetRoots(std::move(roots));
        workspace->Theme().Apply(workspace->CurrentThemeSettings());
    }

    void PluginManager::LoadEnabledState()
    {
        m_enabledOverrides.clear();
        std::ifstream file(AppPaths::ConfigFile("plugins", "enabled.tsv"), std::ios::binary);
        std::string line;
        while (std::getline(file, line))
        {
            const auto parts = SettingsFile::SplitTabs(line);
            bool enabled = true;
            if (parts.size() >= 2 && SettingsFile::ParseBool(SettingsFile::Unescape(parts[1]), &enabled))
            {
                m_enabledOverrides[SettingsFile::Unescape(parts[0])] = enabled;
            }
        }
    }

    void PluginManager::SaveEnabledState() const
    {
        std::ostringstream text;
        for (const auto& [id, enabled] : m_enabledOverrides)
        {
            text << SettingsFile::Escape(id) << '\t' << SettingsFile::SerializeBool(enabled) << '\n';
        }
        const fs::path path = AppPaths::ConfigFile("plugins", "enabled.tsv");
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        DocumentService::AtomicWriteText(path, text.str());
    }

    std::uint64_t PluginManager::ScriptStamp(const PluginInstance& plugin) const
    {
        std::uint64_t stamp = 0;
        const auto consider = [&stamp](const fs::path& path) {
            std::error_code ec;
            const auto time = fs::last_write_time(path, ec);
            if (!ec)
            {
                stamp = std::max<std::uint64_t>(stamp, static_cast<std::uint64_t>(time.time_since_epoch().count()));
            }
        };

        if (plugin.info.source == PluginSource::UserInit)
        {
            consider(plugin.info.directory / fs::u8path(plugin.info.manifest.main));
            return stamp;
        }

        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(plugin.info.directory, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            const auto extension = PathUtils::ToLower(it->path().extension().u8string());
            if (extension == ".lua" || extension == ".json" || extension == ".tsv")
            {
                consider(it->path());
            }
        }
        return stamp;
    }
}
