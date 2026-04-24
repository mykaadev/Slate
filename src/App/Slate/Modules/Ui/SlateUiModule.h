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
        /** Returns module identity for shared UI state. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Creates and registers UI state plus module-owned overlays. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;

    private:
        /** Shared UI state kept while old route adapters are migrated. */
        std::shared_ptr<Software::Slate::SlateUiState> m_state;
        /** Search overlay controller owned by the UI module. */
        std::shared_ptr<Software::Slate::SlateSearchOverlayController> m_searchOverlay;
        /** Workspace switcher overlay owned by the UI module. */
        std::shared_ptr<Software::Slate::SlateWorkspaceSwitcherOverlay> m_workspaceSwitcherOverlay;
    };
}
