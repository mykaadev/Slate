#include "Features/SlateEditor/SlateEditorFeature.h"

#include "Platform/PlatformWindowService.h"

namespace Software::Features::SlateEditor
{
    void SlateEditorFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        if (const auto platformWindow = context.services.Resolve<Software::Platform::PlatformWindowService>())
        {
            m_state->AttachToNativeWindow(platformWindow->NativeHandle());
        }
        context.services.Register<Software::Slate::SlateEditorContext>(m_state);
    }
}
