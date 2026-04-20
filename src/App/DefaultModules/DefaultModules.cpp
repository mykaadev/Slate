#include "DefaultModules.h"

#include <memory>

#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/ToolRegistry.h"

#include "Modes/Slate/SlateAppMode.h"

namespace Software::App::DefaultModules
{
    void Register(Software::Core::Runtime::AppContext& InContext)
    {
        InContext.tools.Register("Slate", []() {
            return std::make_unique<Software::Modes::Slate::SlateAppMode>();
        });

        InContext.tools.Activate("Slate", InContext);
    }
}
