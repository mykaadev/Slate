#pragma once

#include "Core/Runtime/Module.h"

namespace Software::Slate::Modules
{
    /** Registers built-in Slate mode routes with the tool registry. */
    class SlateRouteModule final : public Software::Core::Runtime::IModule
    {
    public:
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;
    };
}
