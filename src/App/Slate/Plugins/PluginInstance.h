#pragma once

#include "App/Slate/Plugins/PluginManager.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct lua_State;

namespace Software::Slate
{
    // One scheduled plugin callback
    struct PluginTimer
    {
        // Stores the timer id returned to Lua
        int id = 0;
        // Stores when the timer fires next, in app seconds
        double due = 0.0;
        // Stores the repeat interval in seconds
        double interval = 0.0;
        // Marks repeating timers
        bool repeat = false;
        // Stores the Lua function reference
        int ref = -1;
    };

    // Live state for one plugin; owned by PluginManager
    struct PluginInstance
    {
        // Stores the public snapshot
        PluginInfo info;
        // Points back at the owning manager
        PluginManager* manager = nullptr;
        // Stores the sandboxed Lua state while loaded
        lua_State* L = nullptr;
        // Changes on every load so stale callbacks are ignored after a reload
        std::uint64_t generation = 0;
        // Stores event listeners by event name
        std::map<std::string, std::vector<int>> events;
        // Stores scheduled timers
        std::vector<PluginTimer> timers;
        // Stores the next timer id
        int nextTimerId = 1;
        // Stores the newest script write time seen at load
        std::uint64_t scriptStamp = 0;
        // Stores the steady-clock deadline for the running call, or zero
        double callDeadline = 0.0;
        // Stores persisted plugin settings
        std::map<std::string, std::string> settings;
        // Marks whether settings were read from disk
        bool settingsLoaded = false;
    };

    namespace PluginLuaApi
    {
        // Opens the sandboxed standard library and installs the slate table
        void Install(PluginInstance& plugin);
        // Removes everything the plugin contributed to shared registries
        void RemoveContributions(PluginInstance& plugin);
        // Installs slate.view, slate.todos, and the ui widget table handed to view draw functions
        void InstallViews(PluginInstance& plugin);
        // Returns steady-clock seconds used for call budgets
        double SteadySeconds();
    }
}
