#pragma once

#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/ModuleRegistry.h"

namespace Software::App::DefaultModules
{
    // Registers the built-in Slate modules that compose the app.
    void Register(Software::Core::Runtime::ModuleRegistry& modules,
                  Software::Core::Runtime::ModuleContext& moduleContext,
                  Software::Core::Runtime::AppContext& context);
}
