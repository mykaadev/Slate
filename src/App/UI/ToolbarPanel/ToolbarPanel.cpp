#include "ToolbarPanel.h"

#include "imgui.h"

#include "Core/Runtime/ToolRegistry.h"

namespace Software::UI::Toolbar
{
    const char* ToolbarPanel::Id() const
    {
        return "Toolbar";
    }

    void ToolbarPanel::Draw(Software::Core::Runtime::AppContext& context)
    {
        if (!ImGui::Begin(Id()))
        {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Modes");
        ImGui::Spacing();

        const ImVec2 buttonSize(ImGui::GetContentRegionAvail().x, 32.0f);
        for (const auto& listing : context.tools.List())
        {
            const bool wasActive = listing.bActive;
            if (wasActive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            if (ImGui::Button(listing.name.c_str(), buttonSize))
            {
                context.tools.Activate(listing.name, context);
            }

            if (wasActive)
            {
                ImGui::PopStyleColor(2);
            }
        }

        ImGui::End();
    }
}
