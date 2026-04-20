#include "EventBus.h"

namespace Software::Core::Runtime
{
    void EventBus::Clear()
    {
        m_listeners.clear();
    }
}
