#include "DefaultModules.h"

#include <memory>

#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateUiState.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/ToolRegistry.h"
#include "Features/SlateEditor/SlateEditorFeature.h"
#include "Features/SlateUi/SlateUiFeature.h"
#include "Features/SlateWorkspace/SlateWorkspaceFeature.h"
#include "Modes/Slate/SlateBrowserMode.h"
#include "Modes/Slate/SlateEditorMode.h"
#include "Modes/Slate/SlateHomeMode.h"
#include "Modes/Slate/SlateSettingsMode.h"
#include "Modes/Slate/SlateWorkspaceMode.h"

namespace Software::App::DefaultModules
{
    void Register(Software::Core::Runtime::AppContext& InContext)
    {
        InContext.features.Register("SlateWorkspace", []() {
            return std::make_shared<Software::Features::SlateWorkspace::SlateWorkspaceFeature>();
        }, InContext);
        InContext.features.Register("SlateEditor", []() {
            return std::make_shared<Software::Features::SlateEditor::SlateEditorFeature>();
        }, InContext);
        InContext.features.Register("SlateUi", []() {
            return std::make_shared<Software::Features::SlateUi::SlateUiFeature>();
        }, InContext);

        InContext.features.SetEnabled("SlateWorkspace", true, InContext);
        InContext.features.SetEnabled("SlateEditor", true, InContext);
        InContext.features.SetEnabled("SlateUi", true, InContext);

        InContext.tools.Register(Software::Slate::ModeIds::Workspace, []() {
            return std::make_unique<Software::Modes::Slate::SlateWorkspaceMode>();
        });
        InContext.tools.Register(Software::Slate::ModeIds::Home, []() {
            return std::make_unique<Software::Modes::Slate::SlateHomeMode>();
        });
        InContext.tools.Register(Software::Slate::ModeIds::Browser, []() {
            return std::make_unique<Software::Modes::Slate::SlateBrowserMode>();
        });
        InContext.tools.Register(Software::Slate::ModeIds::Editor, []() {
            return std::make_unique<Software::Modes::Slate::SlateEditorMode>();
        });
        InContext.tools.Register(Software::Slate::ModeIds::Settings, []() {
            return std::make_unique<Software::Modes::Slate::SlateSettingsMode>();
        });

        const auto workspace = InContext.services.Resolve<Software::Slate::SlateWorkspaceContext>();
        const auto ui = InContext.services.Resolve<Software::Slate::SlateUiState>();
        if (workspace && ui && workspace->HasWorkspaceLoaded())
        {
            ui->ResetForWorkspace(workspace->Workspace());
            ui->workspaceView = Software::Slate::SlateWorkspaceView::Switcher;
            InContext.tools.Activate(Software::Slate::ModeIds::Home, InContext);
        }
        else
        {
            if (ui)
            {
                ui->workspaceView = Software::Slate::SlateWorkspaceView::Setup;
            }
            InContext.tools.Activate(Software::Slate::ModeIds::Workspace, InContext);
        }
    }
}
