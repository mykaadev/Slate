#include "App/Slate/Modules/Routes/SlateRouteModule.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "Core/Runtime/ToolRegistry.h"
#include "Modes/Slate/SlateBrowserMode.h"
#include "Modes/Slate/SlateEditorMode.h"
#include "Modes/Slate/SlateHomeMode.h"
#include "Modes/Slate/SlateSettingsMode.h"
#include "Modes/Slate/SlateWorkspaceMode.h"

#include <memory>

namespace Software::Slate::Modules
{
    Software::Core::Runtime::ModuleDescriptor SlateRouteModule::Describe() const
    {
        return {"slate.routes", "Slate Mode Routes", "1.0.0", {"slate.commands"}};
    }

    void SlateRouteModule::Register(Software::Core::Runtime::ModuleContext& context)
    {
        context.tools.Register(Software::Slate::ModeIds::Workspace, []() {
            return std::make_unique<Software::Modes::Slate::SlateWorkspaceMode>();
        });
        context.tools.Register(Software::Slate::ModeIds::Home, []() {
            return std::make_unique<Software::Modes::Slate::SlateHomeMode>();
        });
        context.tools.Register(Software::Slate::ModeIds::Browser, []() {
            return std::make_unique<Software::Modes::Slate::SlateBrowserMode>();
        });
        context.tools.Register(Software::Slate::ModeIds::Editor, []() {
            return std::make_unique<Software::Modes::Slate::SlateEditorMode>();
        });
        context.tools.Register(Software::Slate::ModeIds::Settings, []() {
            return std::make_unique<Software::Modes::Slate::SlateSettingsMode>();
        });
    }
}
