#include "DetailsPanel.h"

#include "imgui.h"

#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/ToolRegistry.h"

namespace Software::UI::Details
{
    const char* DetailsPanel::Id() const
    {
        return "Details";
    }

    void DetailsPanel::Draw(Software::Core::Runtime::AppContext& context)
    {
        if (!ImGui::Begin(Id()))
        {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Active Tool");
        ImGui::Separator();

        if (!context.tools.ActiveName().empty())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(context.tools.ActiveName().c_str());
            ImGui::PopTextWrapPos();
            ImGui::Separator();
            context.tools.DrawDetails(context);
        }
        else
        {
            ImGui::TextUnformatted("None");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Enabled Features");
        ImGui::Separator();

        bool anyEnabled = false;
        for (const auto& listing : context.features.List())
        {
            if (listing.bEnabled)
            {
                anyEnabled = true;
                break;
            }
        }

        if (anyEnabled)
        {
            context.features.DrawDetails(context);
        }
        else
        {
            ImGui::TextUnformatted("None");
        }

        ImGui::End();
    }
}
