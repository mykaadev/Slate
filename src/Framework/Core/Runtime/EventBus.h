#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Software::Core::Runtime
{
    /**
     * Minimal synchronous event bus. Consumers may subscribe lambdas keyed by event type indices.
     */
    class EventBus
    {
    // Functions
    public:
    
        /** Listener signature for a given event payload. */
        template <typename TEvent>
        using Listener = std::function<void(const TEvent&)>;

        /** Registers a listener for the templated event type. */
        template <typename TEvent>
        void Subscribe(Listener<TEvent> listener)
        {
            const std::type_index key(typeid(TEvent));
            auto& bucket = m_listeners[key];
            bucket.emplace_back([fn = std::move(listener)](const void* payload) 
            {
                fn(*static_cast<const TEvent*>(payload));
            });
        }

        /** Publishes an event to all subscribed listeners. */
        template <typename TEvent>
        void Publish(const TEvent& event)
        {
            const std::type_index key(typeid(TEvent));
            auto it = m_listeners.find(key);
            if (it == m_listeners.end())
            {
                return;
            }

            for (const auto& listener : it->second)
            {
                listener(&event);
            }
        }

        /** Removes all listeners. */
        void Clear();

    // Variables
    private:

        /** Type-erased listener storage keyed by event type. */
        using ErasedListener = std::function<void(const void*)>;
        std::unordered_map<std::type_index, std::vector<ErasedListener>> m_listeners;
    };
}
