#include "Features/Sample/SampleFeature.h"

#include "imgui.h"

#include "Services/ISettings.h"
#include "Services/ILogger.h"

namespace Software::Features::Sample
{
    void SampleFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        if (auto settings = context.services.Resolve<Software::Services::ISettings>())
        {
            m_counter = static_cast<int>(settings->GetInt("sample.counter", 0));
        }

        if (auto logger = context.services.Resolve<Software::Services::ILogger>())
        {
            logger->Info("Sample feature enabled.");
        }
    }

    void SampleFeature::DrawUI(Software::Core::Runtime::AppContext& context)
    {
        if (!bShowWindow)
        {
            return;
        }

        if (ImGui::Begin("Sample Feature", &bShowWindow))
        {
            ImGui::TextUnformatted("This is a placeholder feature you can delete or repurpose.");
            ImGui::Text("Counter: %d", m_counter);

            if (ImGui::Button("Increment"))
            {
                ++m_counter;
                if (auto settings = context.services.Resolve<Software::Services::ISettings>())
                {
                    settings->Set("sample.counter", static_cast<std::int64_t>(m_counter));
                }
            }
        }
        ImGui::End();
    }

    void SampleFeature::DrawDetails(Software::Core::Runtime::AppContext& context)
    {
        if (ImGui::CollapsingHeader("Sample Feature"))
        {
            ImGui::Text("Counter: %d", m_counter);
            ImGui::Text("Window: %s", bShowWindow ? "open" : "closed");
            (void)context;
        }
    }
}
