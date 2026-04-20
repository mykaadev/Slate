#include "Application.h"

#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_internal.h"

#include "App/DefaultModules/DefaultModules.h"
#include "Core/Runtime/EventBus.h"
#include "Core/Runtime/FeatureRegistry.h"
#include "Core/Runtime/InputRouter.h"
#include "Core/Runtime/ServiceLocator.h"
#include "Core/Runtime/ToolRegistry.h"
#include "Platform/PlatformLayer.h"

namespace Software::Core::App
{
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

        std::vector<std::string> droppedFiles;
        platform.SetFileDropCallback([&droppedFiles](std::vector<std::string> files) {
            droppedFiles.insert(droppedFiles.end(), files.begin(), files.end());
        });

        Software::Core::Runtime::AppContext context{services, tools, features, events, {}, false, &droppedFiles};
        Software::App::DefaultModules::Register(context);

        double previousTime = glfwGetTime();
        context.frame.elapsedSeconds = previousTime;
        context.frame.deltaSeconds = 0.0;
        context.frame.frameIndex = 0;

        while (!platform.ShouldClose() && !context.quitRequested)
        {
            platform.PollEvents();

            const double now = glfwGetTime();
            context.frame.deltaSeconds = now - previousTime;
            context.frame.elapsedSeconds = now;
            context.frame.frameIndex += 1;
            previousTime = now;

            inputRouter.SetContext(&context);

            platform.BeginFrame();

            features.Update(context);
            tools.Update(context);

            features.DrawWorld(context);
            tools.DrawWorld(context);

            features.DrawUI(context);
            tools.DrawUI(context);

            platform.EndFrame();
        }

        platform.Shutdown();
        services.Clear();
        events.Clear();

        return 0;
    }
}
