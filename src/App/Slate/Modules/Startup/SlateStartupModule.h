#pragma once

#include "Core/Runtime/Module.h"

namespace Software::Slate::Modules
{
    /** Chooses the first Slate route after services and routes are available. */
    class SlateStartupModule final : public Software::Core::Runtime::IModule
    {
    public:
        /** Returns module identity for startup orchestration. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Reserves startup dependencies without side effects. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;
        /** Activates the initial Slate route after registration completes. */
        void Start(Software::Core::Runtime::ModuleContext& context) override;
    };
}
