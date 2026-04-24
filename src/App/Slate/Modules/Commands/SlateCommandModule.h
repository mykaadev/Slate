#pragma once

#include "Core/Runtime/Module.h"

namespace Software::Slate::Modules
{
    /** Registers the stable built-in command surface. */
    class SlateCommandModule final : public Software::Core::Runtime::IModule
    {
    public:
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;
    };
}
