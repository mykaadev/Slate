#include "Features/Slate/Workspace/SlateWorkspaceFeature.h"

namespace Software::Features::SlateWorkspace
{
    // Registers workspace services and loads saved state
    void SlateWorkspaceFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        context.services.Register<Software::Slate::SlateWorkspaceContext>(m_state);
        m_state->Initialize();
    }

    // Lets background workspace work advance every frame
    void SlateWorkspaceFeature::Update(Software::Core::Runtime::AppContext& context)
    {
        m_state->Update(context.frame.elapsedSeconds);
    }
}
