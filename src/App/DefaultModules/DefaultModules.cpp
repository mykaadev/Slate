#include "DefaultModules.h"

#include <memory>

#include "App/Slate/Modules/Commands/SlateCommandModule.h"
#include "App/Slate/Modules/Editor/SlateEditorModule.h"
#include "App/Slate/Modules/Routes/SlateRouteModule.h"
#include "App/Slate/Modules/Startup/SlateStartupModule.h"
#include "App/Slate/Modules/Todos/SlateTodoModule.h"
#include "App/Slate/Modules/Ui/SlateUiModule.h"
#include "App/Slate/Modules/Workspace/SlateWorkspaceModule.h"

namespace Software::App::DefaultModules
{
    void Register(Software::Core::Runtime::ModuleRegistry& modules,
                  Software::Core::Runtime::ModuleContext& moduleContext,
                  Software::Core::Runtime::AppContext&)
    {
        modules.Register(std::make_unique<Software::Slate::Modules::SlateWorkspaceModule>(), moduleContext);
        modules.Register(std::make_unique<Software::Slate::Modules::SlateEditorModule>(), moduleContext);
        modules.Register(std::make_unique<Software::Slate::Modules::SlateUiModule>(), moduleContext);
        modules.Register(std::make_unique<Software::Slate::Modules::SlateTodoModule>(), moduleContext);
        modules.Register(std::make_unique<Software::Slate::Modules::SlateCommandModule>(), moduleContext);
        modules.Register(std::make_unique<Software::Slate::Modules::SlateRouteModule>(), moduleContext);
        modules.Register(std::make_unique<Software::Slate::Modules::SlateStartupModule>(), moduleContext);
    }
}
