#include "Modes/Slate/SlateViewMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    const char* SlateViewMode::ModeName() const
    {
        return Software::Slate::ModeIds::View;
    }

    void SlateViewMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& ui = UiState(context);
        auto& shortcuts = WorkspaceContext(context).Shortcuts();
        const auto registry = context.services.Resolve<Software::Slate::SlateExtensionRegistry>();
        const auto* view = registry ? registry->FindView(ui.activeView) : nullptr;

        if (handleInput && !ImGui::IsAnyItemActive() && shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
        {
            ShowHome(context);
            return;
        }

        if (!view || !view->draw)
        {
            ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.3f));
            TextCentered(("view not available: " + ui.activeView).c_str(), Muted);
            TextCentered("it may belong to a disabled plugin", Muted);
            return;
        }

        // Header: view title, plus the owning plugin when it adds information
        std::string owner = "plugin " + view->owner;
        std::string lowerTitle = view->title;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (view->owner.empty() || view->owner == lowerTitle)
        {
            owner.clear();
        }
        ScreenHeader(view->title.c_str(), nullptr, owner.c_str(), MarkdownImage);

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float height = std::max(1.0f, available.y - Tokens::FooterHeight + 20.0f);
        ImGui::BeginChild("PluginPageView", ImVec2(available.x, height), false, ImGuiWindowFlags_NoBackground);
        // Copy the callback: the view may re-register itself while drawing
        const auto draw = view->draw;
        draw(available.x, height);
        ImGui::EndChild();
    }

    std::string SlateViewMode::StatusBadge(const Software::Core::Runtime::AppContext& context) const
    {
        auto& mutableContext = const_cast<Software::Core::Runtime::AppContext&>(context);
        const auto registry = mutableContext.services.Resolve<Software::Slate::SlateExtensionRegistry>();
        const auto* view = registry ? registry->FindView(UiState(mutableContext).activeView) : nullptr;
        std::string badge = view ? view->title : std::string("VIEW");
        std::transform(badge.begin(), badge.end(), badge.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return badge;
    }

    std::string SlateViewMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(const_cast<Software::Core::Runtime::AppContext&>(context)).Shortcuts();
        return shortcuts.Helper(Software::Slate::ShortcutAction::CommandPalette, "commands") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
    }
}
