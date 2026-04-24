#pragma once

#include <string>
#include <vector>

namespace Software::Core::Runtime
{
    class CommandRegistry;
    class EventBus;
    class FeatureRegistry;
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
        ServiceLocator& services;
        ToolRegistry& tools;
        FeatureRegistry& features;
        EventBus& events;
        CommandRegistry& commands;
        AppContext& app;
    };

    /** Small lifecycle object that registers services, commands, routes, panels, or jobs. */
    class IModule
    {
    public:
        virtual ~IModule() = default;

        virtual ModuleDescriptor Describe() const = 0;
        virtual void Register(ModuleContext& context) = 0;
        virtual void Start(ModuleContext&) {}
        virtual void Update(ModuleContext&) {}
        virtual void Stop(ModuleContext&) {}
    };
}
