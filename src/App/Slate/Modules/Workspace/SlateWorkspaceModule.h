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
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;
        void Update(Software::Core::Runtime::ModuleContext& context) override;

    private:
        std::shared_ptr<Software::Slate::SlateWorkspaceContext> m_workspace;
    };
}
