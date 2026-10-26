#pragma once

#include "Core/Runtime/Module.h"

#include <memory>

namespace Software::Slate
{
    class PluginManager;
}

namespace Software::Slate::Modules
{
    /** Owns the Lua plugin runtime and the commands that manage plugins. */
    class SlatePluginModule final : public Software::Core::Runtime::IModule
    {
    public:
        /** Returns module identity for the plugin runtime. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Registers the plugin manager service and plugin management commands. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;
        /** Discovers and loads plugins once the workspace and routes are live. */
        void Start(Software::Core::Runtime::ModuleContext& context) override;
        /** Runs plugin timers, events, key bindings, and hot reload. */
        void Update(Software::Core::Runtime::ModuleContext& context) override;
        /** Unloads every plugin. */
        void Stop(Software::Core::Runtime::ModuleContext& context) override;

    private:
        /** Plugin runtime shared through the service locator. */
        std::shared_ptr<Software::Slate::PluginManager> m_plugins;
    };
}
