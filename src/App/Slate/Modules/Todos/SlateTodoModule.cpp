#include "App/Slate/Modules/Todos/SlateTodoModule.h"

#include "App/Slate/Todos/TodoService.h"
#include "App/Slate/Overlays/SlateTodoOverlayController.h"
#include "Core/Runtime/ServiceLocator.h"

namespace Software::Slate::Modules
{
    Software::Core::Runtime::ModuleDescriptor SlateTodoModule::Describe() const
    {
        return {"slate.todos", "Slate Todo Services", "1.0.0", {"slate.workspace"}};
    }

    void SlateTodoModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        m_todos = std::make_shared<Software::Slate::TodoService>();
        m_overlay = std::make_shared<Software::Slate::SlateTodoOverlayController>();

        context.services.Register<Software::Slate::TodoService>(m_todos);
        context.services.Register<Software::Slate::SlateTodoOverlayController>(m_overlay);
    }
}
