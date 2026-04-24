#pragma once

#include "Core/Runtime/Module.h"

namespace Software::Slate::Modules
{
    /** Registers built-in Slate mode routes with the tool registry. */
    class SlateRouteModule final : public Software::Core::Runtime::IModule
    {
    public:
        /** Returns module identity for route registration. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Registers built-in route adapters in the tool registry. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;
    };
}
