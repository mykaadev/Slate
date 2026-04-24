#pragma once

#include "Core/Runtime/Module.h"

#include <memory>

namespace Software::Slate
{
    class SlateSearchOverlayController;
    class SlateUiState;
    class SlateWorkspaceSwitcherOverlay;
}

namespace Software::Slate::Modules
{
    /** Owns app-level Slate UI state until individual UI panels get their own stores. */
    class SlateUiModule final : public Software::Core::Runtime::IModule
    {
    public:
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;

    private:
        std::shared_ptr<Software::Slate::SlateUiState> m_state;
        std::shared_ptr<Software::Slate::SlateSearchOverlayController> m_searchOverlay;
        std::shared_ptr<Software::Slate::SlateWorkspaceSwitcherOverlay> m_workspaceSwitcherOverlay;
    };
}
