#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Software::Core::Runtime
{
    /** Simple type keyed service locator used to share feature owned services. */
    class ServiceLocator
    {
        // Functions
    public:
        /** Registers or replaces the provided service instance. */
        template <typename T>
        void Register(std::shared_ptr<T> service)
        {
            const std::type_index key(typeid(T));
            m_services[key] = std::move(service);
        }

        /** Retrieves a service instance if present. */
        template <typename T>
        std::shared_ptr<T> Resolve() const
        {
            const std::type_index key(typeid(T));
            auto it = m_services.find(key);
            if (it == m_services.end())
            {
                return nullptr;
            }
            return std::static_pointer_cast<T>(it->second);
        }

        /** Checks whether a service is registered. */
        template <typename T>
        bool Contains() const
        {
            const std::type_index key(typeid(T));
            return m_services.find(key) != m_services.end();
        }

        /** Clears all tracked services. */
        void Clear();

        // Variables
    private:
        /** Storage for service instances keyed by type. */
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_services;
    };
}
