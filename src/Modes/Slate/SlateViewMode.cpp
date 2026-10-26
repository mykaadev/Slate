#include "Modes/Slate/SlateViewMode.h"

#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Extensions/SlateExtensionRegistry.h"
#include "App/Slate/State/SlateUiState.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

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

        ImGui::TextColored(Cyan, "%s", view->title.c_str());
        ImGui::SameLine();
        ImGui::TextColored(Muted, "  %s", view->owner.c_str());
        auto* drawList = ImGui::GetWindowDrawList();
        const ImVec2 lineStart(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, ImGui::GetCursorScreenPos().y + 4.0f);
        drawList->AddLine(lineStart,
                          ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x, lineStart.y),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(Muted.x, Muted.y, Muted.z, 0.18f)), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, 12.0f));

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float height = std::max(1.0f, available.y - 46.0f);
        ImGui::BeginChild("PluginPageView", ImVec2(available.x, height), false, ImGuiWindowFlags_NoBackground);
        // Copy the callback: the view may re-register itself while drawing
        const auto draw = view->draw;
        draw(available.x, height);
        ImGui::EndChild();
    }

    std::string SlateViewMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(const_cast<Software::Core::Runtime::AppContext&>(context)).Shortcuts();
        return shortcuts.Helper(Software::Slate::ShortcutAction::CommandPalette, "commands") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
    }
}
