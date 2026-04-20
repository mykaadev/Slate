#include "DefaultModules.h"

#include <memory>

#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/ToolRegistry.h"

#include "Features/Diagnostics/DiagnosticsFeature.h"
#include "Features/Sample/SampleFeature.h"
#include "Modes/Example/ExampleMode.h"

namespace Software::App::DefaultModules
{
    void Register(Software::Core::Runtime::AppContext& InContext)
    {
        InContext.features.Register("Diagnostics", []() {
            return std::make_shared<Software::Features::Diagnostics::DiagnosticsFeature>();
        }, InContext);

        InContext.features.Register("Sample", []() {
            return std::make_shared<Software::Features::Sample::SampleFeature>();
        }, InContext);

        InContext.tools.Register("Example", []() {
            return std::make_unique<Software::Modes::Example::ExampleMode>();
        });

        // Template defaults
        InContext.features.SetEnabled("Diagnostics", true, InContext);
        InContext.features.SetEnabled("Sample", false, InContext);
        InContext.tools.Activate("Example", InContext);
    }
}
