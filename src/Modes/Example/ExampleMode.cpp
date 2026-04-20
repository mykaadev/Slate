#include "Modes/Example/ExampleMode.h"

#include "imgui.h"

#include "Services/ILogger.h"
#include "Services/ISettings.h"

namespace Software::Modes::Example
{
    void ExampleMode::OnEnter(Software::Core::Runtime::AppContext& InContext)
    {
        if (auto Settings = InContext.services.Resolve<Software::Services::ISettings>())
        {
            m_clicks = Settings->GetInt("example.clicks", 0);
        }

        if (auto Logger = InContext.services.Resolve<Software::Services::ILogger>())
        {
            Logger->Info("Example mode activated.");
        }
    }

    void ExampleMode::OnExit(Software::Core::Runtime::AppContext& InContext)
    {
        if (auto Settings = InContext.services.Resolve<Software::Services::ISettings>())
        {
            Settings->Set("example.clicks", m_clicks);
        }

        if (auto Logger = InContext.services.Resolve<Software::Services::ILogger>())
        {
            Logger->Info("Example mode deactivated.");
        }
    }

    Software::Core::Runtime::InputResult ExampleMode::OnMouseButton(const Software::Core::Runtime::MouseButtonEvent& InEvent,
                                                                  Software::Core::Runtime::AppContext& InContext)
    {
        if (InEvent.button == Software::Core::Runtime::MouseButton::Left &&
            InEvent.action == Software::Core::Runtime::InputAction::Press)
        {
            ++m_clicks;

            if (auto Settings = InContext.services.Resolve<Software::Services::ISettings>())
            {
                Settings->Set("example.clicks", m_clicks);
            }

            if (auto Logger = InContext.services.Resolve<Software::Services::ILogger>())
            {
                Logger->Trace("Mouse click recorded by ExampleMode.");
            }
        }

        (void)InContext;
        return Software::Core::Runtime::InputResult::Ignored;
    }

    void ExampleMode::DrawUI(Software::Core::Runtime::AppContext& InContext)
    {
        (void)InContext;

        if (!m_bShowWindow)
        {
            return;
        }

        if (ImGui::Begin("Example Mode", &m_bShowWindow))
        {
            ImGui::TextUnformatted("This window belongs to the currently active tool/mode.");
            ImGui::Text("Clicks: %lld", static_cast<long long>(m_clicks));
        }
        ImGui::End();
    }

    void ExampleMode::DrawDetails(Software::Core::Runtime::AppContext& InContext)
    {
        (void)InContext;

        ImGui::TextUnformatted("Example Mode");
        ImGui::Separator();
        ImGui::Text("Clicks: %lld", static_cast<long long>(m_clicks));
        ImGui::TextUnformatted("Tip: left-click anywhere to increase the counter.");
    }
}
