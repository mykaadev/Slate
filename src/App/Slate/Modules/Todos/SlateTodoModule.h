#pragma once

#include "Core/Runtime/Module.h"

#include <memory>

namespace Software::Slate
{
    class SlateTodoOverlayController;
    class TodoService;
}

namespace Software::Slate::Modules
{
    /** Registers todo-domain services. */
    class SlateTodoModule final : public Software::Core::Runtime::IModule
    {
    public:
        /** Returns module identity for todo services. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Creates and registers todo service and overlay controller. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;

    private:
        /** Shared todo domain service owned by the module. */
        std::shared_ptr<Software::Slate::TodoService> m_todos;
        /** Shared todo overlay controller owned by the module. */
        std::shared_ptr<Software::Slate::SlateTodoOverlayController> m_overlay;
    };
}
