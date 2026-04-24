#pragma once

#include "Core/Runtime/Module.h"

namespace Software::Slate::Modules
{
    /** Registers the stable built-in command surface. */
    class SlateCommandModule final : public Software::Core::Runtime::IModule
    {
    public:
        /** Returns module identity for command registration. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Registers built-in command definitions and aliases. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;
    };
}
