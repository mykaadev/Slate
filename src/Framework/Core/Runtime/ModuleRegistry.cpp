#include "ModuleRegistry.h"

#include <algorithm>
#include <utility>

namespace Software::Core::Runtime
{
    bool ModuleRegistry::Register(std::unique_ptr<IModule> module, ModuleContext& context)
    {
        if (!module)
        {
            return false;
        }

        ModuleDescriptor descriptor = module->Describe();
        if (descriptor.id.empty() || Contains(descriptor.id))
        {
            return false;
        }

        for (const std::string& dependency : descriptor.dependencies)
        {
            if (!Contains(dependency))
            {
                return false;
            }
        }

        module->Register(context);

        Entry entry;
        entry.descriptor = std::move(descriptor);
        entry.module = std::move(module);
        m_entries.push_back(std::move(entry));
        return true;
    }

    void ModuleRegistry::StartAll(ModuleContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (!entry.started && entry.module)
            {
                entry.module->Start(context);
                entry.started = true;
            }
        }
    }

    void ModuleRegistry::UpdateAll(ModuleContext& context)
    {
        for (Entry& entry : m_entries)
        {
            if (entry.started && entry.module)
            {
                entry.module->Update(context);
            }
        }
    }

    void ModuleRegistry::StopAll(ModuleContext& context)
    {
        for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
        {
            if (it->started && it->module)
            {
                it->module->Stop(context);
                it->started = false;
            }
        }
    }

    void ModuleRegistry::Clear()
    {
        m_entries.clear();
    }

    bool ModuleRegistry::Contains(const std::string& id) const
    {
        return Find(id) != nullptr;
    }

    std::vector<ModuleDescriptor> ModuleRegistry::List() const
    {
        std::vector<ModuleDescriptor> result;
        result.reserve(m_entries.size());
        for (const Entry& entry : m_entries)
        {
            result.push_back(entry.descriptor);
        }
        return result;
    }

    ModuleRegistry::Entry* ModuleRegistry::FindMutable(const std::string& id)
    {
        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.descriptor.id == id;
        });
        return it == m_entries.end() ? nullptr : &(*it);
    }

    const ModuleRegistry::Entry* ModuleRegistry::Find(const std::string& id) const
    {
        const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry& entry) {
            return entry.descriptor.id == id;
        });
        return it == m_entries.end() ? nullptr : &(*it);
    }
}
