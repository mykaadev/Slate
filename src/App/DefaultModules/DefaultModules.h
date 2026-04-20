#pragma once

#include "Core/Runtime/AppContext.h"

/** Registers the default set of features and tools for the sample application. */
namespace Software::App::DefaultModules
{
// Functions

    /** Adds default modules to the provided application context. */
    void Register(Software::Core::Runtime::AppContext& InContext);
}
