#include "App/Slate/Modules/Workspace/SlateWorkspaceModule.h"

#include "App/Slate/State/SlateWorkspaceContext.h"
#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/ServiceLocator.h"

namespace Software::Slate::Modules
{
    Software::Core::Runtime::ModuleDescriptor SlateWorkspaceModule::Describe() const
    {
        return {"slate.workspace", "Slate Workspace Services", "1.0.0", {}};
    }

    void SlateWorkspaceModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        m_workspace = std::make_shared<Software::Slate::SlateWorkspaceContext>();
        context.services.Register<Software::Slate::SlateWorkspaceContext>(m_workspace);
        m_workspace->Initialize();
    }

    void SlateWorkspaceModule::Update(Software::Core::Runtime::ModuleContext& context)
    {
        (void)context;
        if (m_workspace)
        {
            m_workspace->Update(context.app.frame.elapsedSeconds);
        }
    }
}
