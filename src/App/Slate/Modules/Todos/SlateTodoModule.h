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
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;

    private:
        std::shared_ptr<Software::Slate::TodoService> m_todos;
        std::shared_ptr<Software::Slate::SlateTodoOverlayController> m_overlay;
    };
}
