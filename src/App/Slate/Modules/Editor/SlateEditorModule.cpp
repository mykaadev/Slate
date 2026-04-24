#include "App/Slate/Modules/Editor/SlateEditorModule.h"

#include "App/Slate/Editor/SlateEditorContext.h"
#include "Core/Runtime/ServiceLocator.h"
#include "Platform/PlatformWindowService.h"

namespace Software::Slate::Modules
{
    Software::Core::Runtime::ModuleDescriptor SlateEditorModule::Describe() const
    {
        return {"slate.editor", "Slate Editor Services", "1.0.0", {}};
    }

    void SlateEditorModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        m_editor = std::make_shared<Software::Slate::SlateEditorContext>();
        if (const auto platformWindow = context.services.Resolve<Software::Platform::PlatformWindowService>())
        {
            m_editor->AttachToNativeWindow(platformWindow->NativeHandle());
        }
        context.services.Register<Software::Slate::SlateEditorContext>(m_editor);
    }
}
