#include "Features/SlateUi/SlateUiFeature.h"

namespace Software::Features::SlateUi
{
    void SlateUiFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        context.services.Register<Software::Slate::SlateUiState>(m_state);
    }
}
