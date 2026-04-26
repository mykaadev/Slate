#include "Features/Slate/Ui/SlateUiFeature.h"

namespace Software::Features::SlateUi
{
    // Registers the shared UI state for the app
    void SlateUiFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        context.services.Register<Software::Slate::SlateUiState>(m_state);
    }
}
