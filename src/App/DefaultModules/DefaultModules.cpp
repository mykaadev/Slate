#include "DefaultModules.h"

#include <memory>

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/ToolRegistry.h"
#include "Features/Slate/Editor/SlateEditorFeature.h"
#include "Features/Slate/Ui/SlateUiFeature.h"
#include "Features/Slate/Workspace/SlateWorkspaceFeature.h"
#include "Modes/Slate/SlateBrowserMode.h"
#include "Modes/Slate/SlateEditorMode.h"
#include "Modes/Slate/SlateHomeMode.h"
#include "Modes/Slate/SlateSettingsMode.h"
#include "Modes/Slate/SlateWorkspaceMode.h"

namespace Software::App::DefaultModules
{
    // Registers the live Slate features
    void Register(Software::Core::Runtime::AppContext& context)
    {
        context.features.Register("SlateWorkspace", []() {
            return std::make_shared<Software::Features::SlateWorkspace::SlateWorkspaceFeature>();
        }, context);
        context.features.Register("SlateEditor", []() {
            return std::make_shared<Software::Features::SlateEditor::SlateEditorFeature>();
        }, context);
        context.features.Register("SlateUi", []() {
            return std::make_shared<Software::Features::SlateUi::SlateUiFeature>();
        }, context);

        // Enables the core feature set right away
        context.features.SetEnabled("SlateWorkspace", true, context);
        context.features.SetEnabled("SlateEditor", true, context);
        context.features.SetEnabled("SlateUi", true, context);

        // Registers the mode stack used by the app
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

        // Chooses the first mode based on workspace state
        const auto workspace = context.services.Resolve<Software::Slate::SlateWorkspaceContext>();
        const auto ui = context.services.Resolve<Software::Slate::SlateUiState>();
        if (workspace && ui && workspace->HasWorkspaceLoaded())
        {
            ui->ResetForWorkspace(workspace->Workspace());
            ui->workspaceView = Software::Slate::SlateWorkspaceView::Switcher;
            context.tools.Activate(Software::Slate::ModeIds::Home, context);
        }
        else
        {
            if (ui)
            {
                ui->workspaceView = Software::Slate::SlateWorkspaceView::Setup;
            }
            context.tools.Activate(Software::Slate::ModeIds::Workspace, context);
        }
    }
}
