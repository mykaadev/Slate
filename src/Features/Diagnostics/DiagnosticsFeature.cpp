#include "Features/Diagnostics/DiagnosticsFeature.h"

#include "Core/Runtime/ToolRegistry.h"
#include "Core/Runtime/FeatureRegistry.h"

#include "imgui.h"

#include "Services/ConsoleLogger.h"
#include "Services/ILogger.h"
#include "Services/ISettings.h"
#include "Services/SettingsStore.h"

#include <sstream>

namespace Software::Features::Diagnostics
{
    void DiagnosticsFeature::OnEnable(Software::Core::Runtime::AppContext& context)
    {
        if (!context.services.Contains<Software::Services::ILogger>())
        {
            context.services.Register<Software::Services::ILogger>(std::make_shared<Software::Services::ConsoleLogger>());
        }

        if (!context.services.Contains<Software::Services::ISettings>())
        {
            auto settings = std::make_shared<Software::Services::SettingsStore>();
            settings->Set("ui.diagnostics.show_demo", false);
            settings->Set("ui.diagnostics.show_log", true);
            context.services.Register<Software::Services::ISettings>(std::move(settings));
        }

        // Pull initial values from settings (if present)
        if (auto settings = context.services.Resolve<Software::Services::ISettings>())
        {
            bShowImGuiDemo = settings->GetBool("ui.diagnostics.show_demo", false);
            bShowLog = settings->GetBool("ui.diagnostics.show_log", true);
        }

        if (auto logger = context.services.Resolve<Software::Services::ILogger>())
        {
            logger->Info("Diagnostics feature enabled.");
        }
    }

    void DiagnosticsFeature::DrawUI(Software::Core::Runtime::AppContext& context)
    {
        if (!bShowWindow)
        {
            return;
        }

        if (ImGui::Begin("Diagnostics", &bShowWindow))
        {
            const double dt = context.frame.deltaSeconds;
            const double fps = (dt > 0.0) ? (1.0 / dt) : 0.0;

            ImGui::Text("Frame: %llu", static_cast<unsigned long long>(context.frame.frameIndex));
            ImGui::Text("dt: %.3f ms", static_cast<float>(dt * 1000.0));
            ImGui::Text("fps: %.1f", static_cast<float>(fps));
            ImGui::Separator();

            ImGui::Text("Last mouse: (%.1f, %.1f)", m_lastMouseX, m_lastMouseY);
            ImGui::Text("Last key: %d (action=%d)", m_lastKey, m_lastKeyAction);
            ImGui::Separator();

            if (ImGui::Checkbox("Show ImGui Demo Window", &bShowImGuiDemo))
            {
                if (auto settings = context.services.Resolve<Software::Services::ISettings>())
                {
                    settings->Set("ui.diagnostics.show_demo", bShowImGuiDemo);
                }
            }

            if (ImGui::Checkbox("Show Log in Details panel", &bShowLog))
            {
                if (auto settings = context.services.Resolve<Software::Services::ISettings>())
                {
                    settings->Set("ui.diagnostics.show_log", bShowLog);
                }
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Registered Tools:");
            for (const auto& listing : context.tools.List())
            {
                ImGui::BulletText("%s%s", listing.name.c_str(), listing.bActive ? " (active)" : "");
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Registered Features:");
            for (const auto& listing : context.features.List())
            {
                ImGui::BulletText("%s%s", listing.name.c_str(), listing.bEnabled ? " (enabled)" : "");
            }
        }
        ImGui::End();

        if (bShowImGuiDemo)
        {
            ImGui::ShowDemoWindow(&bShowImGuiDemo);
        }
    }

    void DiagnosticsFeature::DrawDetails(Software::Core::Runtime::AppContext& context)
    {
        if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Enabled: true");
            ImGui::Text("Log visible: %s", bShowLog ? "yes" : "no");

            if (auto settings = context.services.Resolve<Software::Services::ISettings>())
            {
                if (ImGui::TreeNode("Settings"))
                {
                    for (const auto& key : settings->Keys())
                    {
                        auto v = settings->TryGet(key);
                        if (!v)
                        {
                            continue;
                        }

                        ImGui::BulletText("%s", key.c_str());
                    }
                    ImGui::TreePop();
                }
            }

            if (bShowLog)
            {
                if (auto logger = context.services.Resolve<Software::Services::ILogger>())
                {
                    if (ImGui::TreeNode("Log"))
                    {
                        for (const auto& msg : logger->Snapshot())
                        {
                            ImGui::TextUnformatted(msg.message.c_str());
                        }
                        ImGui::TreePop();
                    }
                }
            }
        }
    }

    Software::Core::Runtime::InputResult DiagnosticsFeature::OnKey(const Software::Core::Runtime::KeyEvent& event,
                                                                 Software::Core::Runtime::AppContext& context)
    {
        m_lastKey = event.key;
        m_lastKeyAction = static_cast<int>(event.action);

        if (event.action == Software::Core::Runtime::InputAction::Press)
        {
            if (auto logger = context.services.Resolve<Software::Services::ILogger>())
            {
                std::ostringstream ss;
                ss << "Key press: key=" << event.key << " scancode=" << event.scancode << " mods=" << event.mods;
                logger->Trace(ss.str());
            }
        }

        return Software::Core::Runtime::InputResult::Ignored;
    }

    Software::Core::Runtime::InputResult DiagnosticsFeature::OnMouseMove(const Software::Core::Runtime::MouseMoveEvent& event,
                                                                       Software::Core::Runtime::AppContext& context)
    {
        (void)context;
        m_lastMouseX = event.position.x;
        m_lastMouseY = event.position.y;
        return Software::Core::Runtime::InputResult::Ignored;
    }
}
