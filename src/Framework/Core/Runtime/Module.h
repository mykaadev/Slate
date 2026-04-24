#pragma once

#include <string>
#include <vector>

namespace Software::Core::Runtime
{
    class CommandRegistry;
    class EventBus;
    class ServiceLocator;
    struct AppContext;
    class ToolRegistry;

    /** Metadata used to identify and reason about app modules. */
    struct ModuleDescriptor
    {
        /** Stable module id, for example slate.documents. */
        std::string id;

        /** Human-readable name. */
        std::string displayName;

        /** Module version. */
        std::string version = "1.0.0";

        /** Stable ids this module expects to be registered first. */
        std::vector<std::string> dependencies;
    };

    /** Registration surface exposed to modules. */
    struct ModuleContext
    {
        /** Shared service registry used to publish or resolve cross-module services. */
        ServiceLocator& services;

        /** Mode/tool registry used by route modules to register user-facing screens. */
        ToolRegistry& tools;

        /** Event bus used for cross-module notifications without direct ownership. */
        EventBus& events;

        /** Command registry used to expose actions to palettes, shortcuts, and modules. */
        CommandRegistry& commands;

        /** Frame/application context shared with existing modes during migration. */
        AppContext& app;
    };

    /** Small lifecycle object that registers services, commands, routes, panels, or jobs. */
    class IModule
    {
    public:
        /** Allows modules to clean up owned services through a base pointer. */
        virtual ~IModule() = default;

        /** Returns stable module metadata before registration. */
        virtual ModuleDescriptor Describe() const = 0;
        /** Registers services, commands, routes, panels, or jobs. */
        virtual void Register(ModuleContext& context) = 0;
        /** Starts runtime behavior after all modules have registered. */
        virtual void Start(ModuleContext&) {}
        /** Advances module-owned background work once per frame. */
        virtual void Update(ModuleContext&) {}
        /** Stops runtime behavior before services are cleared. */
        virtual void Stop(ModuleContext&) {}
    };
}
