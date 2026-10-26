#pragma once

#include "App/Slate/Core/SlateTypes.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Software::Core::Runtime
{
    struct AppContext;
}

namespace Software::Slate
{
    // Where a plugin was discovered; later sources override earlier ones with the same id
    enum class PluginSource
    {
        Bundled,
        User,
        Workspace,
        UserInit
    };

    // Lifecycle state of one plugin
    enum class PluginStatus
    {
        Disabled,
        Loaded,
        Failed
    };

    // Kind of value a plugin setting holds
    enum class PluginSettingType
    {
        Bool,
        Number,
        Choice,
        Text
    };

    // One user-facing setting declared in plugin.json; the settings screen renders it
    struct PluginSettingSpec
    {
        // Stores the key passed to slate.settings.get
        std::string key;
        // Stores the visible label
        std::string label;
        // Stores a one line hint
        std::string description;
        // Stores the value kind
        PluginSettingType type = PluginSettingType::Bool;
        // Stores the default in stored form: b:true, n:220, s:monday
        std::string defaultValue;
        // Stores the choices for Choice settings
        std::vector<std::string> options;
        // Stores the range and step for Number settings
        double min = 0.0;
        double max = 100.0;
        double step = 1.0;
    };

    // Metadata read from plugin.json
    struct PluginManifest
    {
        // Stores the stable plugin id, for example word-count
        std::string id;
        // Stores the visible name
        std::string name;
        // Stores the plugin version
        std::string version = "0.1.0";
        // Stores a one line description
        std::string description;
        // Stores the author name
        std::string author;
        // Stores the entry script relative to the plugin folder
        std::string main = "main.lua";
        // Stores the Slate plugin API version the plugin targets
        int apiVersion = 1;
        // Stores the settings the plugin exposes on the settings screen
        std::vector<PluginSettingSpec> settings;
    };

    // Read-only snapshot of one plugin for settings screens and commands
    struct PluginInfo
    {
        // Stores the manifest
        PluginManifest manifest;
        // Stores the plugin folder
        fs::path directory;
        // Stores where the plugin came from
        PluginSource source = PluginSource::User;
        // Marks whether the user enabled it
        bool enabled = true;
        // Stores the lifecycle state
        PluginStatus status = PluginStatus::Disabled;
        // Stores the last load or runtime error
        std::string error;
        // Stores recent log lines written by the plugin
        std::vector<std::string> log;
        // Stores command ids the plugin registered
        std::vector<std::string> commands;
    };

    struct PluginInstance;

    // Discovers, sandboxes, loads, hot reloads, and dispatches events to Lua plugins
    class PluginManager
    {
    public:
        // Current plugin API version exposed as slate.api_version
        static constexpr int ApiVersion = 1;

        PluginManager();
        ~PluginManager();

        // Subscribes to app events and loads every enabled plugin
        void Start(Software::Core::Runtime::AppContext& context);
        // Runs timers, change events, key bindings, and hot reload checks
        void Update(Software::Core::Runtime::AppContext& context);
        // Unloads every plugin
        void Stop();

        // Returns a snapshot of every discovered plugin
        std::vector<PluginInfo> Plugins() const;
        // Enables or disables a plugin and persists the choice
        bool SetEnabled(const std::string& id, bool enabled, std::string* error = nullptr);
        // Reloads one plugin from disk
        bool Reload(const std::string& id, std::string* error = nullptr);
        // Rediscovers plugin folders and reloads everything
        void ReloadAll();
        // Creates a starter plugin in the user plugin folder
        bool CreatePlugin(const std::string& name, fs::path* createdDirectory, std::string* error = nullptr);

        // Returns a plugin setting in stored form (b:, n:, s:), falling back to the manifest default
        std::string SettingValue(const std::string& id, const std::string& key);
        // Stores a plugin setting, saves it, and sends settings_changed to the plugin
        bool SetSetting(const std::string& id, const std::string& key, const std::string& stored, std::string* error = nullptr);
        // Clears every stored setting of a plugin back to its defaults
        bool ResetSettings(const std::string& id, std::string* error = nullptr);
        // Formats stored values for display and parses them back
        static std::string StoredBool(bool value);
        static std::string StoredNumber(double value);
        static std::string StoredText(const std::string& value);
        static bool StoredAsBool(const std::string& stored);
        static double StoredAsNumber(const std::string& stored);
        static std::string StoredAsText(const std::string& stored);

        // Returns the bundled plugin folder next to the executable
        static fs::path BundledPluginDirectory();
        // Returns the user plugin folder
        static fs::path UserPluginDirectory();
        // Returns the workspace plugin folder for the open workspace, or empty
        fs::path WorkspacePluginDirectory() const;

        // Internal entry points used by the Lua bindings
        Software::Core::Runtime::AppContext* App() const;
        // Logs one line for a plugin and mirrors errors to the status line
        void Log(PluginInstance& plugin, const std::string& message, bool isError = false);
        // Calls the function on top of the stack with a time budget; pops it and its arguments
        bool Call(PluginInstance& plugin, int argumentCount, int resultCount, std::string* error = nullptr);
        // Calls a stored Lua function reference if its plugin generation still matches
        bool CallRef(const std::string& pluginId, std::uint64_t generation, int ref,
                     const std::function<int(PluginInstance&)>& pushArguments, int resultCount,
                     const std::function<void(PluginInstance&)>& readResults = {});
        // Finds a loaded plugin by id
        PluginInstance* FindLoaded(const std::string& id);
        // Schedules a timer callback
        int AddTimer(PluginInstance& plugin, double delaySeconds, bool repeat, int ref);
        // Cancels a timer by id
        void CancelTimer(PluginInstance& plugin, int timerId);
        // Requests that theme preset roots be recomputed
        void MarkThemesDirty();

    private:
        struct Candidate
        {
            PluginManifest manifest;
            fs::path directory;
            PluginSource source = PluginSource::User;
        };

        std::vector<Candidate> Discover() const;
        void LoadAll();
        void UnloadAll();
        bool LoadPlugin(PluginInstance& plugin);
        void UnloadPlugin(PluginInstance& plugin);
        void Emit(const std::string& eventName, const std::function<int(PluginInstance&)>& pushArguments);
        void EmitBeforeSave(const fs::path& relativePath, std::string* text);
        void RunTimers(double now);
        void CheckDocumentChange(double now);
        void CheckKeyBindings();
        void CheckHotReload(double now);
        void SyncThemeRoots();
        void LoadEnabledState();
        void SaveEnabledState() const;
        std::uint64_t ScriptStamp(const PluginInstance& plugin) const;

        Software::Core::Runtime::AppContext* m_app = nullptr;
        std::vector<std::unique_ptr<PluginInstance>> m_plugins;
        std::map<std::string, bool> m_enabledOverrides;
        fs::path m_loadedWorkspaceRoot;
        double m_lastReloadCheck = 0.0;
        double m_lastSeenEditSeconds = -1.0;
        fs::path m_lastSeenEditPath;
        bool m_documentChangePending = false;
        bool m_themesDirty = false;
        bool m_started = false;
        std::uint64_t m_nextGeneration = 1;
    };
}
