#include "App/Slate/Plugins/PluginInstance.h"

#include "App/Slate/Core/AppPaths.h"
#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Core/SettingsFile.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/Extensions/SlateShellBridge.h"
#include "App/Slate/Input/ShortcutService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"

#include "Core/Runtime/AppConfig.h"
#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/CommandRegistry.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        constexpr const char* InstanceKey = "slate.instance";
        constexpr const char* ModulesKey = "slate.modules";

        // ---------------------------------------------------------------- helpers

        PluginInstance& Self(lua_State* L)
        {
            lua_getfield(L, LUA_REGISTRYINDEX, InstanceKey);
            auto* plugin = static_cast<PluginInstance*>(lua_touserdata(L, -1));
            lua_pop(L, 1);
            return *plugin;
        }

        Software::Core::Runtime::AppContext& App(lua_State* L)
        {
            auto* app = Self(L).manager->App();
            if (!app)
            {
                luaL_error(L, "slate is not running");
            }
            return *app;
        }

        template <typename T>
        std::shared_ptr<T> Service(lua_State* L)
        {
            auto service = App(L).services.Resolve<T>();
            if (!service)
            {
                luaL_error(L, "slate service is not available yet");
            }
            return service;
        }

        std::string CheckString(lua_State* L, int index)
        {
            std::size_t length = 0;
            const char* value = luaL_checklstring(L, index, &length);
            return std::string(value, length);
        }

        std::string OptString(lua_State* L, int index, const std::string& fallback = {})
        {
            if (lua_isnoneornil(L, index))
            {
                return fallback;
            }
            return CheckString(L, index);
        }

        void PushString(lua_State* L, const std::string& value)
        {
            lua_pushlstring(L, value.data(), value.size());
        }

        std::string FieldString(lua_State* L, int table, const char* key, const std::string& fallback = {})
        {
            lua_getfield(L, table, key);
            std::string value = fallback;
            if (lua_type(L, -1) == LUA_TSTRING)
            {
                std::size_t length = 0;
                const char* text = lua_tolstring(L, -1, &length);
                value.assign(text, length);
            }
            lua_pop(L, 1);
            return value;
        }

        bool FieldBool(lua_State* L, int table, const char* key, bool fallback)
        {
            lua_getfield(L, table, key);
            const bool value = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            return value;
        }

        int FieldFunctionRef(lua_State* L, int table, const char* key)
        {
            lua_getfield(L, table, key);
            if (lua_isfunction(L, -1))
            {
                return luaL_ref(L, LUA_REGISTRYINDEX);
            }
            lua_pop(L, 1);
            return LUA_NOREF;
        }

        std::vector<std::string> FieldStringList(lua_State* L, int table, const char* key)
        {
            std::vector<std::string> values;
            lua_getfield(L, table, key);
            if (lua_istable(L, -1))
            {
                const lua_Integer count = luaL_len(L, -1);
                for (lua_Integer i = 1; i <= count; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    if (lua_type(L, -1) == LUA_TSTRING)
                    {
                        values.emplace_back(lua_tostring(L, -1));
                    }
                    lua_pop(L, 1);
                }
            }
            else if (lua_type(L, -1) == LUA_TSTRING)
            {
                values.emplace_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
            return values;
        }

        int CheckFunctionRef(lua_State* L, int index)
        {
            luaL_checktype(L, index, LUA_TFUNCTION);
            lua_pushvalue(L, index);
            return luaL_ref(L, LUA_REGISTRYINDEX);
        }

        std::string JoinArguments(lua_State* L, int first)
        {
            std::string message;
            const int top = lua_gettop(L);
            for (int i = first; i <= top; ++i)
            {
                std::size_t length = 0;
                const char* text = luaL_tolstring(L, i, &length);
                if (i > first)
                {
                    message.push_back(' ');
                }
                message.append(text, length);
                lua_pop(L, 1);
            }
            return message;
        }

        std::string QualifiedId(const PluginInstance& plugin, const std::string& id)
        {
            const std::string prefix = plugin.info.manifest.id + ".";
            return id.rfind(prefix, 0) == 0 ? id : prefix + id;
        }

        bool IsValidToken(const std::string& value)
        {
            return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
                return std::isalnum(ch) != 0 || ch == '-' || ch == '_';
            });
        }

        // Resolves a workspace relative path and refuses anything that escapes the workspace root
        fs::path ResolveInsideWorkspace(lua_State* L, const std::string& relative)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (!workspace->HasWorkspaceLoaded())
            {
                luaL_error(L, "no workspace is open");
            }

            std::error_code ec;
            const fs::path root = fs::weakly_canonical(workspace->Workspace().Root(), ec);
            const fs::path target = fs::weakly_canonical(root / fs::u8path(relative), ec);
            const auto rootText = root.generic_u8string();
            const auto targetText = target.generic_u8string();
            if (ec || targetText.size() < rootText.size() || targetText.compare(0, rootText.size(), rootText) != 0 ||
                (targetText.size() > rootText.size() && targetText[rootText.size()] != '/'))
            {
                luaL_error(L, "path escapes the workspace: %s", relative.c_str());
            }
            return target;
        }

        // Keeps a deferred callback alive across reloads by matching the plugin generation
        struct DeferredRef
        {
            PluginManager* manager = nullptr;
            std::string pluginId;
            std::uint64_t generation = 0;
            int ref = LUA_NOREF;
        };

        DeferredRef MakeDeferred(lua_State* L, int index)
        {
            PluginInstance& plugin = Self(L);
            return DeferredRef{plugin.manager, plugin.info.manifest.id, plugin.generation, CheckFunctionRef(L, index)};
        }

        // ---------------------------------------------------------------- plugin settings storage

        fs::path PluginSettingsPath(const PluginInstance& plugin)
        {
            return AppPaths::ConfigFile("plugins", plugin.info.manifest.id + ".tsv");
        }

        void LoadSettingsOnce(PluginInstance& plugin)
        {
            if (plugin.settingsLoaded)
            {
                return;
            }
            plugin.settingsLoaded = true;
            std::ifstream file(PluginSettingsPath(plugin), std::ios::binary);
            std::string line;
            while (std::getline(file, line))
            {
                const auto parts = SettingsFile::SplitTabs(line);
                if (parts.size() >= 2)
                {
                    plugin.settings[SettingsFile::Unescape(parts[0])] = SettingsFile::Unescape(parts[1]);
                }
            }
        }

        void WriteSettings(const PluginInstance& plugin)
        {
            std::ostringstream text;
            for (const auto& [key, value] : plugin.settings)
            {
                text << SettingsFile::Escape(key) << '\t' << SettingsFile::Escape(value) << '\n';
            }
            const fs::path path = PluginSettingsPath(plugin);
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            DocumentService::AtomicWriteText(path, text.str());
        }

        // Values keep their Lua type with a one letter prefix
        void PushStored(lua_State* L, const std::string& stored)
        {
            if (stored.size() >= 2 && stored[1] == ':')
            {
                const std::string body = stored.substr(2);
                switch (stored[0])
                {
                case 'b':
                    lua_pushboolean(L, body == "true");
                    return;
                case 'n':
                    lua_pushnumber(L, std::strtod(body.c_str(), nullptr));
                    return;
                case 's':
                    PushString(L, body);
                    return;
                default:
                    break;
                }
            }
            PushString(L, stored);
        }

        std::string StoreValue(lua_State* L, int index)
        {
            switch (lua_type(L, index))
            {
            case LUA_TBOOLEAN:
                return lua_toboolean(L, index) ? "b:true" : "b:false";
            case LUA_TNUMBER:
            {
                std::ostringstream text;
                text.precision(17);
                text << lua_tonumber(L, index);
                return "n:" + text.str();
            }
            case LUA_TSTRING:
                return "s:" + CheckString(L, index);
            default:
                luaL_error(L, "settings only store strings, numbers, and booleans");
                return {};
            }
        }

        // ---------------------------------------------------------------- core

        int L_Log(lua_State* L)
        {
            PluginInstance& plugin = Self(L);
            plugin.manager->Log(plugin, JoinArguments(L, 1));
            return 0;
        }

        int L_Notify(lua_State* L)
        {
            Service<SlateShellBridge>(L)->Notify(JoinArguments(L, 1));
            return 0;
        }

        int L_Error(lua_State* L)
        {
            Service<SlateShellBridge>(L)->Notify(JoinArguments(L, 1), true);
            return 0;
        }

        int L_Time(lua_State* L)
        {
            lua_pushnumber(L, App(L).frame.elapsedSeconds);
            return 1;
        }

        int L_OpenUrl(lua_State* L)
        {
            const std::string url = CheckString(L, 1);
            const std::string lower = PathUtils::ToLower(url);
            if (lower.rfind("https://", 0) != 0 && lower.rfind("http://", 0) != 0 && lower.rfind("mailto:", 0) != 0)
            {
                return luaL_error(L, "open_url only opens http, https, and mailto links");
            }
#if defined(_WIN32)
            const std::wstring wide = fs::u8path(url).wstring();
            ::ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
            return 0;
        }

        int L_On(lua_State* L)
        {
            const std::string eventName = CheckString(L, 1);
            const int ref = CheckFunctionRef(L, 2);
            Self(L).events[eventName].push_back(ref);
            return 0;
        }

        int L_Require(lua_State* L)
        {
            const std::string name = CheckString(L, 1);
            lua_getfield(L, LUA_REGISTRYINDEX, ModulesKey);
            lua_getfield(L, -1, name.c_str());
            if (!lua_isnil(L, -1))
            {
                return 1;
            }
            lua_pop(L, 1);

            std::string relative = name;
            std::replace(relative.begin(), relative.end(), '.', '/');
            if (relative.find("..") != std::string::npos)
            {
                return luaL_error(L, "invalid module name: %s", name.c_str());
            }

            const PluginInstance& plugin = Self(L);
            const fs::path path = plugin.info.directory / fs::u8path(relative + ".lua");
            std::string source;
            if (!DocumentService::ReadTextFile(path, &source))
            {
                return luaL_error(L, "module not found: %s", name.c_str());
            }

            const std::string chunkName = "@" + relative + ".lua";
            if (luaL_loadbufferx(L, source.data(), source.size(), chunkName.c_str(), "t") != LUA_OK)
            {
                return lua_error(L);
            }
            PushString(L, name);
            lua_call(L, 1, 1);
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1);
                lua_pushboolean(L, 1);
            }
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, name.c_str());
            return 1;
        }

        // ---------------------------------------------------------------- commands

        int L_CommandRegister(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            PluginInstance& plugin = Self(L);
            auto& commands = App(L).commands;

            const std::string localId = FieldString(L, 1, "id");
            if (localId.empty())
            {
                return luaL_error(L, "command needs an id");
            }
            const int ref = FieldFunctionRef(L, 1, "run");
            if (ref == LUA_NOREF)
            {
                return luaL_error(L, "command '%s' needs a run function", localId.c_str());
            }

            Software::Core::Runtime::CommandDefinition definition;
            definition.id = QualifiedId(plugin, localId);
            definition.label = FieldString(L, 1, "label", localId);
            definition.category = FieldString(L, 1, "category", plugin.info.manifest.name);
            definition.description = FieldString(L, 1, "description");
            definition.acceptsArguments = FieldBool(L, 1, "args", true);

            // Plugins may add aliases but never take one away from another command
            for (const auto& alias : FieldStringList(L, 1, "aliases"))
            {
                Software::Core::Runtime::CommandRequest existing;
                if (commands.Resolve(alias, &existing) && existing.id != definition.id &&
                    std::find(plugin.info.commands.begin(), plugin.info.commands.end(), existing.id) == plugin.info.commands.end())
                {
                    plugin.manager->Log(plugin, "alias '" + alias + "' is taken by " + existing.id);
                    continue;
                }
                definition.aliases.push_back(alias);
            }
            if (definition.aliases.empty())
            {
                definition.aliases.push_back(definition.id);
            }

            PluginManager* manager = plugin.manager;
            const std::string pluginId = plugin.info.manifest.id;
            const std::uint64_t generation = plugin.generation;
            definition.execute = [manager, pluginId, generation, ref](Software::Core::Runtime::AppContext&,
                                                                      const Software::Core::Runtime::CommandRequest& request) {
                std::string message;
                bool succeeded = true;
                const bool called = manager->CallRef(
                    pluginId, generation, ref,
                    [&request](PluginInstance& target) {
                        PushString(target.L, request.arguments);
                        return 1;
                    },
                    1,
                    [&](PluginInstance& target) {
                        if (lua_type(target.L, -1) == LUA_TSTRING)
                        {
                            message = lua_tostring(target.L, -1);
                        }
                        else if (lua_isboolean(target.L, -1) && !lua_toboolean(target.L, -1))
                        {
                            succeeded = false;
                        }
                    });
                if (!called || !succeeded)
                {
                    return Software::Core::Runtime::CommandResult::Failed(request.id, message);
                }
                return Software::Core::Runtime::CommandResult::Handled(request.id, message);
            };

            const std::string id = definition.id;
            commands.Register(std::move(definition));
            if (std::find(plugin.info.commands.begin(), plugin.info.commands.end(), id) == plugin.info.commands.end())
            {
                plugin.info.commands.push_back(id);
            }
            PushString(L, id);
            return 1;
        }

        int L_CommandRun(lua_State* L)
        {
            std::string line = CheckString(L, 1);
            const std::string arguments = OptString(L, 2);
            if (!arguments.empty())
            {
                line += " " + arguments;
            }
            Service<SlateShellBridge>(L)->RunCommand(std::move(line));
            return 0;
        }

        int L_CommandList(lua_State* L)
        {
            const auto definitions = App(L).commands.List();
            lua_createtable(L, static_cast<int>(definitions.size()), 0);
            int index = 1;
            for (const auto& definition : definitions)
            {
                lua_createtable(L, 0, 5);
                PushString(L, definition.id);
                lua_setfield(L, -2, "id");
                PushString(L, definition.label);
                lua_setfield(L, -2, "label");
                PushString(L, definition.category);
                lua_setfield(L, -2, "category");
                PushString(L, definition.description);
                lua_setfield(L, -2, "description");
                lua_createtable(L, static_cast<int>(definition.aliases.size()), 0);
                for (std::size_t i = 0; i < definition.aliases.size(); ++i)
                {
                    PushString(L, definition.aliases[i]);
                    lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
                }
                lua_setfield(L, -2, "aliases");
                lua_rawseti(L, -2, index++);
            }
            return 1;
        }

        // ---------------------------------------------------------------- slash commands, home, status, keys

        int L_SlashRegister(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            PluginInstance& plugin = Self(L);

            SlashExtension command;
            command.owner = plugin.info.manifest.id;
            command.id = PathUtils::ToLower(FieldString(L, 1, "id"));
            if (!IsValidToken(command.id))
            {
                return luaL_error(L, "slash command id must be letters, digits, - or _");
            }
            command.label = FieldString(L, 1, "label", command.id);
            command.description = FieldString(L, 1, "description");

            const std::string insert = FieldString(L, 1, "insert");
            const int ref = FieldFunctionRef(L, 1, "run");
            if (ref == LUA_NOREF && insert.empty())
            {
                return luaL_error(L, "slash command '%s' needs insert text or a run function", command.id.c_str());
            }

            PluginManager* manager = plugin.manager;
            const std::string pluginId = plugin.info.manifest.id;
            const std::uint64_t generation = plugin.generation;
            command.run = [manager, pluginId, generation, ref, insert](std::string* replacement, std::string* error) {
                if (ref == LUA_NOREF)
                {
                    *replacement = insert;
                    return true;
                }
                std::string result;
                const bool called = manager->CallRef(pluginId, generation, ref, {}, 1, [&result](PluginInstance& target) {
                    if (lua_type(target.L, -1) == LUA_TSTRING || lua_type(target.L, -1) == LUA_TNUMBER)
                    {
                        result = lua_tostring(target.L, -1);
                    }
                });
                if (!called)
                {
                    if (error)
                    {
                        *error = pluginId + ": slash command failed";
                    }
                    return false;
                }
                *replacement = result;
                return true;
            };

            Service<SlateExtensionRegistry>(L)->SetSlashCommand(std::move(command));
            return 0;
        }

        int L_HomeAdd(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            PluginInstance& plugin = Self(L);

            HomeEntry entry;
            entry.owner = plugin.info.manifest.id;
            entry.id = FieldString(L, 1, "id");
            entry.label = FieldString(L, 1, "label", entry.id);
            entry.section = FieldString(L, 1, "section", "plugins");
            entry.key = PathUtils::ToLower(FieldString(L, 1, "key"));
            entry.command = FieldString(L, 1, "command");
            if (entry.id.empty())
            {
                return luaL_error(L, "home entry needs an id");
            }

            // A run function becomes a regular command so it also shows in the palette
            lua_getfield(L, 1, "run");
            const bool hasRun = lua_isfunction(L, -1);
            lua_pop(L, 1);
            if (hasRun)
            {
                lua_createtable(L, 0, 3);
                PushString(L, "home." + entry.id);
                lua_setfield(L, -2, "id");
                PushString(L, entry.label);
                lua_setfield(L, -2, "label");
                lua_getfield(L, 1, "run");
                lua_setfield(L, -2, "run");
                lua_pushcfunction(L, L_CommandRegister);
                lua_insert(L, -2);
                lua_call(L, 1, 1);
                entry.command = lua_tostring(L, -1);
                lua_pop(L, 1);
            }
            if (entry.command.empty())
            {
                return luaL_error(L, "home entry '%s' needs a command or run function", entry.id.c_str());
            }

            Service<SlateExtensionRegistry>(L)->SetHomeEntry(std::move(entry));
            return 0;
        }

        int L_StatusSet(lua_State* L)
        {
            StatusItem item;
            item.owner = Self(L).info.manifest.id;
            item.id = CheckString(L, 1);
            item.text = OptString(L, 2);
            Service<SlateExtensionRegistry>(L)->SetStatusItem(std::move(item));
            return 0;
        }

        int L_StatusClear(lua_State* L)
        {
            StatusItem item;
            item.owner = Self(L).info.manifest.id;
            item.id = CheckString(L, 1);
            Service<SlateExtensionRegistry>(L)->SetStatusItem(std::move(item));
            return 0;
        }

        int L_KeymapSet(lua_State* L)
        {
            PluginInstance& plugin = Self(L);
            const std::string chord = CheckString(L, 1);
            KeyBinding binding;
            if (!ShortcutService::ParseBinding(chord, &binding))
            {
                return luaL_error(L, "unknown key chord: %s", chord.c_str());
            }

            CommandKeyBinding keyBinding;
            keyBinding.owner = plugin.info.manifest.id;
            keyBinding.binding = binding;
            if (lua_isfunction(L, 2))
            {
                lua_createtable(L, 0, 3);
                PushString(L, "key." + ShortcutService::SerializeBinding(binding));
                lua_setfield(L, -2, "id");
                PushString(L, chord);
                lua_setfield(L, -2, "label");
                lua_pushvalue(L, 2);
                lua_setfield(L, -2, "run");
                lua_pushcfunction(L, L_CommandRegister);
                lua_insert(L, -2);
                lua_call(L, 1, 1);
                keyBinding.command = lua_tostring(L, -1);
                lua_pop(L, 1);
            }
            else
            {
                keyBinding.command = CheckString(L, 2);
            }
            Service<SlateExtensionRegistry>(L)->SetKeyBinding(std::move(keyBinding));
            return 0;
        }

        // ---------------------------------------------------------------- timers

        int AddTimer(lua_State* L, bool repeat)
        {
            const double seconds = luaL_checknumber(L, 1);
            const int ref = CheckFunctionRef(L, 2);
            PluginInstance& plugin = Self(L);
            lua_pushinteger(L, plugin.manager->AddTimer(plugin, seconds, repeat, ref));
            return 1;
        }

        int L_TimerAfter(lua_State* L)
        {
            return AddTimer(L, false);
        }

        int L_TimerEvery(lua_State* L)
        {
            return AddTimer(L, true);
        }

        int L_TimerCancel(lua_State* L)
        {
            PluginInstance& plugin = Self(L);
            plugin.manager->CancelTimer(plugin, static_cast<int>(luaL_checkinteger(L, 1)));
            return 0;
        }

        // ---------------------------------------------------------------- editor and document

        bool HasDocument(lua_State* L)
        {
            return Service<SlateWorkspaceContext>(L)->Documents().HasOpenDocument();
        }

        void RequireDocument(lua_State* L)
        {
            if (!HasDocument(L))
            {
                luaL_error(L, "no document is open");
            }
        }

        int L_EditorAvailable(lua_State* L)
        {
            lua_pushboolean(L, HasDocument(L));
            return 1;
        }

        int L_EditorText(lua_State* L)
        {
            if (!HasDocument(L))
            {
                lua_pushnil(L);
                return 1;
            }
            auto workspace = Service<SlateWorkspaceContext>(L);
            Service<SlateEditorContext>(L)->CommitToActiveDocument(workspace->Documents(), App(L).frame.elapsedSeconds);
            PushString(L, workspace->Documents().Active()->text);
            return 1;
        }

        int L_EditorSetText(lua_State* L)
        {
            RequireDocument(L);
            const std::string text = CheckString(L, 1);
            auto workspace = Service<SlateWorkspaceContext>(L);
            Service<SlateEditorContext>(L)->ReplaceAllText(workspace->Documents(), text, App(L).frame.elapsedSeconds);
            return 0;
        }

        int L_EditorInsert(lua_State* L)
        {
            RequireDocument(L);
            const std::string text = CheckString(L, 1);
            auto workspace = Service<SlateWorkspaceContext>(L);
            Service<SlateEditorContext>(L)->InsertTextAtCursor(workspace->Documents(), text, App(L).frame.elapsedSeconds);
            return 0;
        }

        int L_EditorSelection(lua_State* L)
        {
            std::string text;
            if (HasDocument(L) && Service<SlateEditorContext>(L)->SelectedText(&text))
            {
                PushString(L, text);
            }
            else
            {
                PushString(L, "");
            }
            return 1;
        }

        int L_EditorLine(lua_State* L)
        {
            std::string text;
            if (HasDocument(L) && Service<SlateEditorContext>(L)->ActiveLineText(&text))
            {
                PushString(L, text);
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }

        int L_EditorSetLine(lua_State* L)
        {
            RequireDocument(L);
            const std::string text = CheckString(L, 1);
            auto workspace = Service<SlateWorkspaceContext>(L);
            Service<SlateEditorContext>(L)->ReplaceActiveLineWithText(workspace->Documents(), text, App(L).frame.elapsedSeconds);
            return 0;
        }

        int L_EditorCaret(lua_State* L)
        {
            std::size_t line = 0;
            std::size_t column = 0;
            if (!HasDocument(L) || !Service<SlateEditorContext>(L)->CaretPosition(&line, &column))
            {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, static_cast<lua_Integer>(line));
            lua_pushinteger(L, static_cast<lua_Integer>(column));
            return 2;
        }

        int L_EditorGoto(lua_State* L)
        {
            RequireDocument(L);
            const lua_Integer line = luaL_checkinteger(L, 1);
            Service<SlateEditorContext>(L)->JumpToLine(static_cast<std::size_t>(std::max<lua_Integer>(1, line)));
            return 0;
        }

        int L_DocumentPath(lua_State* L)
        {
            const auto* document = Service<SlateWorkspaceContext>(L)->Documents().Active();
            if (!document)
            {
                lua_pushnil(L);
                return 1;
            }
            PushString(L, document->relativePath.generic_u8string());
            return 1;
        }

        int L_DocumentName(lua_State* L)
        {
            const auto* document = Service<SlateWorkspaceContext>(L)->Documents().Active();
            if (!document)
            {
                lua_pushnil(L);
                return 1;
            }
            PushString(L, document->relativePath.stem().u8string());
            return 1;
        }

        int L_DocumentDirty(lua_State* L)
        {
            const auto* document = Service<SlateWorkspaceContext>(L)->Documents().Active();
            lua_pushboolean(L, document && document->dirty);
            return 1;
        }

        int L_DocumentSave(lua_State* L)
        {
            Service<SlateShellBridge>(L)->RunCommand("slate.document.save");
            return 0;
        }

        int L_DocumentOpen(lua_State* L)
        {
            const std::string path = CheckString(L, 1);
            ResolveInsideWorkspace(L, path);
            Service<SlateShellBridge>(L)->RunCommand("slate.document.open " + path);
            return 0;
        }

        // ---------------------------------------------------------------- workspace

        int L_WorkspaceRoot(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (!workspace->HasWorkspaceLoaded())
            {
                lua_pushnil(L);
                return 1;
            }
            PushString(L, workspace->Workspace().Root().u8string());
            return 1;
        }

        int L_WorkspaceName(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (!workspace->HasWorkspaceLoaded())
            {
                lua_pushnil(L);
                return 1;
            }
            if (const auto* vault = workspace->WorkspaceRegistry().ActiveVault())
            {
                PushString(L, vault->title);
            }
            else
            {
                PushString(L, workspace->Workspace().Root().filename().u8string());
            }
            return 1;
        }

        int L_WorkspaceFiles(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (!workspace->HasWorkspaceLoaded())
            {
                lua_newtable(L);
                return 1;
            }
            const auto& files = workspace->Workspace().MarkdownFiles();
            lua_createtable(L, static_cast<int>(files.size()), 0);
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                PushString(L, files[i].generic_u8string());
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
            return 1;
        }

        int L_WorkspaceRead(lua_State* L)
        {
            const fs::path path = ResolveInsideWorkspace(L, CheckString(L, 1));
            std::string text;
            if (!DocumentService::ReadTextFile(path, &text))
            {
                lua_pushnil(L);
                return 1;
            }
            PushString(L, text);
            return 1;
        }

        int L_WorkspaceWrite(lua_State* L)
        {
            const fs::path path = ResolveInsideWorkspace(L, CheckString(L, 1));
            const std::string text = CheckString(L, 2);
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            std::string error;
            if (!DocumentService::AtomicWriteText(path, text, &error))
            {
                return luaL_error(L, "write failed: %s", error.c_str());
            }
            return 0;
        }

        int L_WorkspaceExists(lua_State* L)
        {
            std::error_code ec;
            lua_pushboolean(L, fs::exists(ResolveInsideWorkspace(L, CheckString(L, 1)), ec));
            return 1;
        }

        int L_WorkspaceJournalPath(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (!workspace->HasWorkspaceLoaded())
            {
                lua_pushnil(L);
                return 1;
            }
            PushString(L, workspace->Workspace().DailyNoteRelativePath().generic_u8string());
            return 1;
        }

        int L_WorkspaceRescan(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            if (workspace->HasWorkspaceLoaded())
            {
                workspace->Workspace().Scan();
            }
            return 0;
        }

        // ---------------------------------------------------------------- plugin settings

        int L_SettingsGet(lua_State* L)
        {
            PluginInstance& plugin = Self(L);
            LoadSettingsOnce(plugin);
            const std::string key = CheckString(L, 1);
            const auto it = plugin.settings.find(key);
            if (it != plugin.settings.end())
            {
                PushStored(L, it->second);
                return 1;
            }
            // An explicit fallback wins; otherwise use the default declared in plugin.json
            if (lua_isnoneornil(L, 2))
            {
                for (const auto& spec : plugin.info.manifest.settings)
                {
                    if (spec.key == key)
                    {
                        PushStored(L, spec.defaultValue);
                        return 1;
                    }
                }
            }
            lua_settop(L, 2);
            return 1;
        }

        int L_SettingsSet(lua_State* L)
        {
            PluginInstance& plugin = Self(L);
            LoadSettingsOnce(plugin);
            const std::string key = CheckString(L, 1);
            if (lua_isnoneornil(L, 2))
            {
                plugin.settings.erase(key);
            }
            else
            {
                plugin.settings[key] = StoreValue(L, 2);
            }
            WriteSettings(plugin);
            return 0;
        }

        // ---------------------------------------------------------------- app config

        struct ConfigEntry
        {
            const char* key;
            const char* kind;
            const char* description;
        };

        const std::vector<ConfigEntry>& ConfigEntries()
        {
            static const std::vector<ConfigEntry> entries{
                {"shell_theme", "string", "shell palette preset id"},
                {"markdown_theme", "string", "markdown accent preset id"},
                {"font_family", "string", "editor font family"},
                {"font_size", "number", "editor font size in pixels (8-24)"},
                {"line_spacing", "number", "extra pixels between lines (0-12)"},
                {"page_width", "number", "writing column width in pixels, 0 for fluid"},
                {"live_preview", "boolean", "render markdown in place"},
                {"heading_scale", "boolean", "draw headings larger than body text"},
                {"highlight_line", "boolean", "highlight the caret line"},
                {"tab_width", "number", "indent width (2-8)"},
                {"indent_with_tabs", "boolean", "indent with tab characters"},
                {"smart_lists", "boolean", "continue lists on enter"},
                {"link_underline", "boolean", "underline links"},
                {"paste_images", "boolean", "paste clipboard images as files"},
                {"panel_motion", "number", "0 instant to 3 floaty"},
                {"scroll_motion", "number", "0 instant, 1 smooth, 2 glide"},
                {"scrollbar_style", "number", "0 slim, 1 soft, 2 bold"},
                {"interface_scale", "number", "text size of the whole app in percent (80-150)"},
                {"start_screen", "number", "0 home, 1 last note, 2 today's journal"},
                {"boot_sequence", "boolean", "play the boot sequence on launch"},
                {"status_clock", "boolean", "show the clock in the status bar"},
                {"cursor_blink", "boolean", "blink the caret and logo cursor"},
                {"scanlines", "number", "CRT scanlines: 0 off, 1 subtle, 2 strong"},
            };
            return entries;
        }

        int L_ConfigGet(lua_State* L)
        {
            const std::string key = CheckString(L, 1);
            auto workspace = Service<SlateWorkspaceContext>(L);
            const auto& editor = workspace->CurrentEditorSettings();
            const auto& theme = workspace->CurrentThemeSettings();

            if (key == "shell_theme") PushString(L, theme.shellPreset);
            else if (key == "markdown_theme") PushString(L, theme.markdownPreset);
            else if (key == "font_family") PushString(L, editor.fontFamily);
            else if (key == "font_size") lua_pushinteger(L, editor.fontSize);
            else if (key == "line_spacing") lua_pushinteger(L, editor.lineSpacing);
            else if (key == "page_width") lua_pushinteger(L, editor.pageWidth);
            else if (key == "live_preview") lua_pushboolean(L, editor.livePreview);
            else if (key == "heading_scale") lua_pushboolean(L, editor.headingScale);
            else if (key == "word_wrap") lua_pushboolean(L, editor.wordWrap);
            else if (key == "show_whitespace") lua_pushboolean(L, editor.showWhitespace);
            else if (key == "highlight_line") lua_pushboolean(L, editor.highlightCurrentLine);
            else if (key == "tab_width") lua_pushinteger(L, editor.tabWidth);
            else if (key == "indent_with_tabs") lua_pushboolean(L, editor.indentWithTabs);
            else if (key == "smart_lists") lua_pushboolean(L, editor.autoListContinuation);
            else if (key == "link_underline") lua_pushboolean(L, editor.linkUnderline);
            else if (key == "paste_images") lua_pushboolean(L, editor.pasteClipboardImages);
            else if (key == "side_panels") lua_pushinteger(L, editor.panelLayout);
            else if (key == "preview_sync") lua_pushinteger(L, editor.previewFollowMode);
            else if (key == "panel_motion") lua_pushinteger(L, editor.panelMotion);
            else if (key == "scroll_motion") lua_pushinteger(L, editor.scrollMotion);
            else if (key == "caret_motion") lua_pushinteger(L, editor.caretMotion);
            else if (key == "scrollbar_style") lua_pushinteger(L, editor.scrollbarStyle);
            else if (key == "interface_scale") lua_pushinteger(L, editor.interfaceScale);
            else if (key == "start_screen") lua_pushinteger(L, editor.startScreen);
            else if (key == "boot_sequence") lua_pushboolean(L, editor.bootSequence);
            else if (key == "status_clock") lua_pushboolean(L, editor.statusClock);
            else if (key == "cursor_blink") lua_pushboolean(L, editor.cursorBlink);
            else if (key == "scanlines") lua_pushinteger(L, editor.scanlines);
            else return luaL_error(L, "unknown config key: %s", key.c_str());
            return 1;
        }

        int L_ConfigSet(lua_State* L)
        {
            const std::string key = CheckString(L, 1);
            auto workspace = Service<SlateWorkspaceContext>(L);
            auto editor = workspace->CurrentEditorSettings();
            auto theme = workspace->CurrentThemeSettings();
            const auto integer = [L]() { return static_cast<int>(luaL_checkinteger(L, 2)); };
            const auto boolean = [L]() {
                luaL_checktype(L, 2, LUA_TBOOLEAN);
                return lua_toboolean(L, 2) != 0;
            };

            bool themeChanged = false;
            if (key == "shell_theme") { theme.shellPreset = CheckString(L, 2); themeChanged = true; }
            else if (key == "markdown_theme") { theme.markdownPreset = CheckString(L, 2); themeChanged = true; }
            else if (key == "font_family") editor.fontFamily = CheckString(L, 2);
            else if (key == "font_size") editor.fontSize = integer();
            else if (key == "line_spacing") editor.lineSpacing = integer();
            else if (key == "page_width") editor.pageWidth = integer();
            else if (key == "live_preview") editor.livePreview = boolean();
            else if (key == "heading_scale") editor.headingScale = boolean();
            else if (key == "word_wrap") editor.wordWrap = boolean();
            else if (key == "show_whitespace") editor.showWhitespace = boolean();
            else if (key == "highlight_line") editor.highlightCurrentLine = boolean();
            else if (key == "tab_width") editor.tabWidth = integer();
            else if (key == "indent_with_tabs") editor.indentWithTabs = boolean();
            else if (key == "smart_lists") editor.autoListContinuation = boolean();
            else if (key == "link_underline") editor.linkUnderline = boolean();
            else if (key == "paste_images") editor.pasteClipboardImages = boolean();
            else if (key == "side_panels") editor.panelLayout = integer();
            else if (key == "preview_sync") editor.previewFollowMode = integer();
            else if (key == "panel_motion") editor.panelMotion = integer();
            else if (key == "scroll_motion") editor.scrollMotion = integer();
            else if (key == "caret_motion") editor.caretMotion = integer();
            else if (key == "scrollbar_style") editor.scrollbarStyle = integer();
            else if (key == "interface_scale") editor.interfaceScale = integer();
            else if (key == "start_screen") editor.startScreen = integer();
            else if (key == "boot_sequence") editor.bootSequence = boolean();
            else if (key == "status_clock") editor.statusClock = boolean();
            else if (key == "cursor_blink") editor.cursorBlink = boolean();
            else if (key == "scanlines") editor.scanlines = integer();
            else return luaL_error(L, "unknown config key: %s", key.c_str());

            std::string error;
            if (themeChanged)
            {
                workspace->SetThemeSettings(theme);
                workspace->SaveThemeSettings(&error);
            }
            else
            {
                workspace->SetEditorSettings(editor);
                workspace->SaveEditorSettings(&error);
            }
            if (!error.empty())
            {
                Self(L).manager->Log(Self(L), "config: " + error);
            }
            return 0;
        }

        int L_ConfigKeys(lua_State* L)
        {
            const auto& entries = ConfigEntries();
            lua_createtable(L, static_cast<int>(entries.size()), 0);
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                lua_createtable(L, 0, 3);
                lua_pushstring(L, entries[i].key);
                lua_setfield(L, -2, "key");
                lua_pushstring(L, entries[i].kind);
                lua_setfield(L, -2, "type");
                lua_pushstring(L, entries[i].description);
                lua_setfield(L, -2, "description");
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
            return 1;
        }

        int L_ThemeList(lua_State* L)
        {
            auto workspace = Service<SlateWorkspaceContext>(L);
            const auto pushList = [L](const std::vector<std::string>& ids) {
                lua_createtable(L, static_cast<int>(ids.size()), 0);
                for (std::size_t i = 0; i < ids.size(); ++i)
                {
                    PushString(L, ids[i]);
                    lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
                }
            };
            lua_createtable(L, 0, 2);
            pushList(workspace->Theme().ShellPresetIds());
            lua_setfield(L, -2, "shell");
            pushList(workspace->Theme().MarkdownPresetIds());
            lua_setfield(L, -2, "markdown");
            return 1;
        }

        // ---------------------------------------------------------------- ui

        int L_UiPrompt(lua_State* L)
        {
            const std::string title = CheckString(L, 1);
            const std::string initial = OptString(L, 2);
            const DeferredRef callback = MakeDeferred(L, 3);
            Service<SlateShellBridge>(L)->Prompt(title, initial, [callback](const std::string& value) {
                callback.manager->CallRef(callback.pluginId, callback.generation, callback.ref, [&value](PluginInstance& target) {
                    PushString(target.L, value);
                    return 1;
                }, 0);
            });
            return 0;
        }

        int L_UiConfirm(lua_State* L)
        {
            const std::string message = CheckString(L, 1);
            const DeferredRef callback = MakeDeferred(L, 2);
            Service<SlateShellBridge>(L)->Confirm(message, [callback](bool accepted) {
                callback.manager->CallRef(callback.pluginId, callback.generation, callback.ref, [accepted](PluginInstance& target) {
                    lua_pushboolean(target.L, accepted);
                    return 1;
                }, 0);
            });
            return 0;
        }

        // ---------------------------------------------------------------- install

        void SetFunctions(lua_State* L, const luaL_Reg* functions)
        {
            luaL_setfuncs(L, functions, 0);
        }

        void AddSubtable(lua_State* L, const char* name, const luaL_Reg* functions)
        {
            lua_newtable(L);
            SetFunctions(L, functions);
            lua_setfield(L, -2, name);
        }

        void BudgetHook(lua_State* L, lua_Debug*)
        {
            const PluginInstance& plugin = Self(L);
            if (plugin.callDeadline > 0.0 && PluginLuaApi::SteadySeconds() > plugin.callDeadline)
            {
                luaL_error(L, "plugin took longer than its time budget");
            }
        }

        void OpenSandboxedLibraries(lua_State* L)
        {
            const luaL_Reg libraries[] = {
                {LUA_GNAME, luaopen_base},
                {LUA_TABLIBNAME, luaopen_table},
                {LUA_STRLIBNAME, luaopen_string},
                {LUA_MATHLIBNAME, luaopen_math},
                {LUA_UTF8LIBNAME, luaopen_utf8},
                {LUA_COLIBNAME, luaopen_coroutine},
                {LUA_OSLIBNAME, luaopen_os},
            };
            for (const auto& library : libraries)
            {
                luaL_requiref(L, library.name, library.func, 1);
                lua_pop(L, 1);
            }

            // No file, process, or bytecode access; plugins talk to disk through slate.workspace
            for (const char* name : {"dofile", "loadfile"})
            {
                lua_pushnil(L);
                lua_setglobal(L, name);
            }
            lua_getglobal(L, LUA_OSLIBNAME);
            for (const char* name : {"execute", "exit", "getenv", "remove", "rename", "tmpname", "setlocale"})
            {
                lua_pushnil(L);
                lua_setfield(L, -2, name);
            }
            lua_pop(L, 1);

            static const char* prelude = R"lua(
local raw_load = load
load = function(chunk, name, _, env) return raw_load(chunk, name, "t", env) end
print = slate.log
require = slate.require
)lua";
            if (luaL_loadbufferx(L, prelude, std::strlen(prelude), "=slate", "t") == LUA_OK)
            {
                lua_call(L, 0, 0);
            }
        }
    }

    namespace PluginLuaApi
    {
        double SteadySeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }

        void Install(PluginInstance& plugin)
        {
            lua_State* L = plugin.L;
            lua_pushlightuserdata(L, &plugin);
            lua_setfield(L, LUA_REGISTRYINDEX, InstanceKey);
            lua_newtable(L);
            lua_setfield(L, LUA_REGISTRYINDEX, ModulesKey);
            lua_sethook(L, BudgetHook, LUA_MASKCOUNT, 10000);

            static const luaL_Reg core[] = {
                {"log", L_Log},
                {"notify", L_Notify},
                {"error", L_Error},
                {"time", L_Time},
                {"open_url", L_OpenUrl},
                {"on", L_On},
                {"require", L_Require},
                {nullptr, nullptr},
            };
            static const luaL_Reg command[] = {
                {"register", L_CommandRegister},
                {"run", L_CommandRun},
                {"list", L_CommandList},
                {nullptr, nullptr},
            };
            static const luaL_Reg slash[] = {{"register", L_SlashRegister}, {nullptr, nullptr}};
            static const luaL_Reg home[] = {{"add", L_HomeAdd}, {nullptr, nullptr}};
            static const luaL_Reg status[] = {{"set", L_StatusSet}, {"clear", L_StatusClear}, {nullptr, nullptr}};
            static const luaL_Reg keymap[] = {{"set", L_KeymapSet}, {nullptr, nullptr}};
            static const luaL_Reg timer[] = {
                {"after", L_TimerAfter},
                {"every", L_TimerEvery},
                {"cancel", L_TimerCancel},
                {nullptr, nullptr},
            };
            static const luaL_Reg editor[] = {
                {"available", L_EditorAvailable},
                {"text", L_EditorText},
                {"set_text", L_EditorSetText},
                {"insert", L_EditorInsert},
                {"selection", L_EditorSelection},
                {"replace_selection", L_EditorInsert},
                {"line", L_EditorLine},
                {"set_line", L_EditorSetLine},
                {"caret", L_EditorCaret},
                {"goto", L_EditorGoto},
                {nullptr, nullptr},
            };
            static const luaL_Reg document[] = {
                {"path", L_DocumentPath},
                {"name", L_DocumentName},
                {"dirty", L_DocumentDirty},
                {"save", L_DocumentSave},
                {"open", L_DocumentOpen},
                {nullptr, nullptr},
            };
            static const luaL_Reg workspace[] = {
                {"root", L_WorkspaceRoot},
                {"name", L_WorkspaceName},
                {"files", L_WorkspaceFiles},
                {"read", L_WorkspaceRead},
                {"write", L_WorkspaceWrite},
                {"exists", L_WorkspaceExists},
                {"journal_path", L_WorkspaceJournalPath},
                {"rescan", L_WorkspaceRescan},
                {nullptr, nullptr},
            };
            static const luaL_Reg settings[] = {{"get", L_SettingsGet}, {"set", L_SettingsSet}, {nullptr, nullptr}};
            static const luaL_Reg config[] = {
                {"get", L_ConfigGet},
                {"set", L_ConfigSet},
                {"keys", L_ConfigKeys},
                {nullptr, nullptr},
            };
            static const luaL_Reg theme[] = {{"list", L_ThemeList}, {nullptr, nullptr}};
            static const luaL_Reg ui[] = {{"prompt", L_UiPrompt}, {"confirm", L_UiConfirm}, {nullptr, nullptr}};

            lua_newtable(L);
            SetFunctions(L, core);
            AddSubtable(L, "command", command);
            AddSubtable(L, "slash", slash);
            AddSubtable(L, "home", home);
            AddSubtable(L, "status", status);
            AddSubtable(L, "keymap", keymap);
            AddSubtable(L, "timer", timer);
            AddSubtable(L, "editor", editor);
            AddSubtable(L, "document", document);
            AddSubtable(L, "workspace", workspace);
            AddSubtable(L, "settings", settings);
            AddSubtable(L, "config", config);
            AddSubtable(L, "theme", theme);
            AddSubtable(L, "ui", ui);

            lua_pushstring(L, SOFTWARE_APP_VERSION);
            lua_setfield(L, -2, "version");
            lua_pushinteger(L, PluginManager::ApiVersion);
            lua_setfield(L, -2, "api_version");

            lua_createtable(L, 0, 4);
            PushString(L, plugin.info.manifest.id);
            lua_setfield(L, -2, "id");
            PushString(L, plugin.info.manifest.name);
            lua_setfield(L, -2, "name");
            PushString(L, plugin.info.manifest.version);
            lua_setfield(L, -2, "version");
            PushString(L, plugin.info.directory.u8string());
            lua_setfield(L, -2, "dir");
            lua_setfield(L, -2, "plugin");

            lua_setglobal(L, "slate");
            InstallViews(plugin);
            OpenSandboxedLibraries(L);
        }

        void RemoveContributions(PluginInstance& plugin)
        {
            auto* app = plugin.manager ? plugin.manager->App() : nullptr;
            if (!app)
            {
                return;
            }
            for (const auto& id : plugin.info.commands)
            {
                app->commands.Unregister(id);
            }
            if (const auto registry = app->services.Resolve<SlateExtensionRegistry>())
            {
                registry->RemoveOwner(plugin.info.manifest.id);
            }
        }

        void EnsureSettingsLoaded(PluginInstance& plugin)
        {
            LoadSettingsOnce(plugin);
        }

        void SaveSettings(const PluginInstance& plugin)
        {
            WriteSettings(plugin);
        }

        void PushStoredValue(lua_State* L, const std::string& stored)
        {
            PushStored(L, stored);
        }
    }
}
