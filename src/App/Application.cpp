#include "Application.h"

#include <memory>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_internal.h"

#include "App/DefaultModules/DefaultModules.h"
#include "Core/Runtime/EventBus.h"
#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/InputRouter.h"
#include "Core/Runtime/ServiceLocator.h"
#include "Core/Runtime/ToolRegistry.h"
#include "Core/UI/PanelHost.h"
#include "Platform/PlatformLayer.h"
#include "App/UI/DetailsPanel/DetailsPanel.h"
#include "App/UI/ToolbarPanel/ToolbarPanel.h"
#include "App/UI/TopBarPanel/TopBarPanel.h"

namespace Software::Core::App
{
    namespace
    {
        void DrawDockspace()
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("RootDockspace", nullptr, flags);
            ImGui::PopStyleVar(3);

            const ImGuiID dockspaceId = ImGui::GetID("RootDockspaceID");
            const ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode |
                                                ImGuiDockNodeFlags_AutoHideTabBar;
            ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

            static bool s_layoutInitialised = false;
            if (!s_layoutInitialised)
            {
                s_layoutInitialised = true;

                ImGui::DockBuilderRemoveNode(dockspaceId);
                ImGui::DockBuilderAddNode(dockspaceId,
                                          ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
                ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

                ImGuiID mainNode = dockspaceId;
                ImGuiID leftStrip = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.28f, nullptr, &mainNode);
                ImGuiID detailsNode =
                    ImGui::DockBuilderSplitNode(leftStrip, ImGuiDir_Right, 0.72f, nullptr, &leftStrip);

                ImGui::DockBuilderDockWindow("Toolbar", leftStrip);
                ImGui::DockBuilderDockWindow("Details", detailsNode);
                ImGui::DockBuilderFinish(dockspaceId);
            }

            ImGui::End();
        }
    }

    int Application::Run()
    {
        Software::Platform::PlatformLayer platform;
        if (!platform.Initialize())
        {
            return -1;
        }

        Software::Core::Runtime::ServiceLocator services;
        Software::Core::Runtime::ToolRegistry tools;
        Software::Core::Runtime::FeatureRegistry features;
        Software::Core::Runtime::EventBus events;
        Software::Core::Runtime::InputRouter inputRouter;
        inputRouter.Bind(&tools, &features);
        platform.SetInputRouter(&inputRouter);

        Software::Core::UI::PanelHost panelHost;
        panelHost.Register(std::make_unique<Software::UI::TopBar::TopBarPanel>());
        panelHost.Register(std::make_unique<Software::UI::Toolbar::ToolbarPanel>());
        panelHost.Register(std::make_unique<Software::UI::Details::DetailsPanel>());

        Software::Core::Runtime::AppContext context{services, tools, features, events, {}};
        Software::App::DefaultModules::Register(context);

        double previousTime = glfwGetTime();
        context.frame.elapsedSeconds = previousTime;
        context.frame.deltaSeconds = 0.0;
        context.frame.frameIndex = 0;

        while (!platform.ShouldClose())
        {
            platform.PollEvents();

            const double now = glfwGetTime();
            context.frame.deltaSeconds = now - previousTime;
            context.frame.elapsedSeconds = now;
            context.frame.frameIndex += 1;
            previousTime = now;

            inputRouter.SetContext(&context);

            platform.BeginFrame();
            DrawDockspace();

            features.Update(context);
            tools.Update(context);

            features.DrawWorld(context);
            tools.DrawWorld(context);

            features.DrawUI(context);
            tools.DrawUI(context);

            panelHost.Draw(context);

            platform.EndFrame();
        }

        platform.Shutdown();
        services.Clear();
        events.Clear();

        return 0;
    }
}
