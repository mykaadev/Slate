#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/Runtime/Module.h"

namespace Software::Core::Runtime
{
    /** Owns loaded modules and drives their lifecycle. */
    class ModuleRegistry
    {
    public:
        /** Registers a module and immediately runs its registration hook. */
        bool Register(std::unique_ptr<IModule> module, ModuleContext& context);

        /** Starts all registered modules in registration order. */
        void StartAll(ModuleContext& context);

        /** Updates all started modules in registration order. */
        void UpdateAll(ModuleContext& context);

        /** Stops all registered modules in reverse registration order. */
        void StopAll(ModuleContext& context);

        /** Clears modules without invoking lifecycle hooks. */
        void Clear();

        /** Returns whether a module id is registered. */
        bool Contains(const std::string& id) const;

        /** Lists descriptors in registration order. */
        std::vector<ModuleDescriptor> List() const;

    private:
        struct Entry
        {
            ModuleDescriptor descriptor;
            std::unique_ptr<IModule> module;
            bool started = false;
        };

        Entry* FindMutable(const std::string& id);
        const Entry* Find(const std::string& id) const;

        std::vector<Entry> m_entries;
    };
}
