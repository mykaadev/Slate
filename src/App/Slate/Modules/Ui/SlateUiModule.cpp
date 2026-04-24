#include "App/Slate/Modules/Ui/SlateUiModule.h"

#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/Overlays/SlateSearchOverlayController.h"
#include "App/Slate/Overlays/SlateWorkspaceSwitcherOverlay.h"
#include "Core/Runtime/ServiceLocator.h"

namespace Software::Slate::Modules
{
    Software::Core::Runtime::ModuleDescriptor SlateUiModule::Describe() const
    {
        return {"slate.ui", "Slate UI State and Overlays", "1.0.0", {}};
    }

    void SlateUiModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        m_state = std::make_shared<Software::Slate::SlateUiState>();
        m_searchOverlay = std::make_shared<Software::Slate::SlateSearchOverlayController>();
        m_workspaceSwitcherOverlay = std::make_shared<Software::Slate::SlateWorkspaceSwitcherOverlay>();

        context.services.Register<Software::Slate::SlateUiState>(m_state);
        context.services.Register<Software::Slate::SlateSearchOverlayController>(m_searchOverlay);
        context.services.Register<Software::Slate::SlateWorkspaceSwitcherOverlay>(m_workspaceSwitcherOverlay);
    }
}
