#include "Features/SlateWorkspace/SlateWorkspaceFeature.h"

namespace Software::Features::SlateWorkspace
{
    void SlateWorkspaceFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        context.services.Register<Software::Slate::SlateWorkspaceContext>(m_state);
        m_state->Initialize();
    }

    void SlateWorkspaceFeature::Update(Software::Core::Runtime::AppContext& context)
    {
        m_state->Update(context.frame.elapsedSeconds);
    }
}
