#pragma once

#include "Core/Runtime/Module.h"

#include <memory>

namespace Software::Slate
{
    class SlateWorkspaceContext;
}

namespace Software::Slate::Modules
{
    /** Owns workspace-domain state and background workspace updates. */
    class SlateWorkspaceModule final : public Software::Core::Runtime::IModule
    {
    public:
        /** Returns module identity for workspace services. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Creates and registers the workspace context. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;
        /** Ticks workspace-owned background tasks. */
        void Update(Software::Core::Runtime::ModuleContext& context) override;

    private:
        /** Shared workspace context owned by the module. */
        std::shared_ptr<Software::Slate::SlateWorkspaceContext> m_workspace;
    };
}
