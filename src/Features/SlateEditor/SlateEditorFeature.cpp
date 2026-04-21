#include "Features/SlateEditor/SlateEditorFeature.h"

namespace Software::Features::SlateEditor
{
    void SlateEditorFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        context.services.Register<Software::Slate::SlateEditorContext>(m_state);
    }
}
