#include "App/Slate/Modules/Startup/SlateStartupModule.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "Core/Runtime/ServiceLocator.h"
#include "Core/Runtime/ToolRegistry.h"

namespace Software::Slate::Modules
{
    Software::Core::Runtime::ModuleDescriptor SlateStartupModule::Describe() const
    {
        return {"slate.startup", "Slate Startup", "1.0.0", {"slate.routes"}};
    }

    void SlateStartupModule::Register(Software::Core::Runtime::ModuleContext&)
    {
    }

    void SlateStartupModule::Start(Software::Core::Runtime::ModuleContext& context)
    {
        const auto workspace = context.services.Resolve<Software::Slate::SlateWorkspaceContext>();
        const auto ui = context.services.Resolve<Software::Slate::SlateUiState>();
        if (workspace && ui && workspace->HasWorkspaceLoaded())
        {
            ui->ResetForWorkspace(workspace->Workspace());
            ui->workspaceView = Software::Slate::SlateWorkspaceView::Switcher;
            context.tools.Activate(Software::Slate::ModeIds::Home, context.app);
        }
        else
        {
            if (ui)
            {
                ui->workspaceView = Software::Slate::SlateWorkspaceView::Setup;
            }
            context.tools.Activate(Software::Slate::ModeIds::Workspace, context.app);
        }
    }
}
