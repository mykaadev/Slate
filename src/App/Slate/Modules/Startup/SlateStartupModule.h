#pragma once

#include "Core/Runtime/Module.h"

namespace Software::Slate::Modules
{
    /** Chooses the first Slate route after services and routes are available. */
    class SlateStartupModule final : public Software::Core::Runtime::IModule
    {
    public:
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;
        void Start(Software::Core::Runtime::ModuleContext& context) override;
    };
}
