#include "TopBarPanel.h"

#include "imgui.h"

#include "Core/Runtime/FeatureRegistry.h"

namespace Software::UI::TopBar
{
    const char* TopBarPanel::Id() const
    {
        return "TopBar";
    }

    void TopBarPanel::Draw(Software::Core::Runtime::AppContext& context)
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        if (ImGui::BeginMenu("Features"))
        {
            const auto listings = context.features.List();
            if (listings.empty())
            {
                ImGui::MenuItem("No features registered", nullptr, false, false);
            }
            else
            {
                for (const auto& listing : listings)
                {
                    if (ImGui::BeginMenu(listing.name.c_str()))
                    {
                        ImGui::PushID(listing.name.c_str());

                        bool enabled = listing.bEnabled;
                        if (ImGui::MenuItem("Enabled", nullptr, &enabled))
                        {
                            context.features.SetEnabled(listing.name, enabled, context);
                        }

                        if (context.features.IsEnabled(listing.name))
                        {
                            ImGui::Separator();
                            context.features.DrawDetailsFor(listing.name, context);
                        }

                        ImGui::PopID();
                        ImGui::EndMenu();
                    }
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}
