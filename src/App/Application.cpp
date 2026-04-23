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
#include "Platform/PlatformWindowService.h"

namespace Software::Core::App
{
    // Builds the shared runtime objects and runs the frame loop
    int Application::Run()
    {
        // Owns the native window and frame lifecycle
        Software::Platform::PlatformLayer platform;
        if (!platform.Initialize())
        {
            return -1;
        }

        // Shared services used across features and modes
        Software::Core::Runtime::ServiceLocator services;
        Software::Core::Runtime::ToolRegistry tools;
        Software::Core::Runtime::FeatureRegistry features;
        Software::Core::Runtime::EventBus events;
        Software::Core::Runtime::InputRouter inputRouter;
        inputRouter.Bind(&tools, &features);
        platform.SetInputRouter(&inputRouter);

        // Collects dropped files until the editor consumes them
        std::vector<std::string> droppedFiles;
        platform.SetFileDropCallback([&droppedFiles](std::vector<std::string> files) {
            droppedFiles.insert(droppedFiles.end(), files.begin(), files.end());
        });

        // Carries shared state through the app
        Software::Core::Runtime::AppContext context{services, tools, features, events, {}, false, &droppedFiles};
        services.Register<Software::Platform::PlatformWindowService>(
            std::make_shared<Software::Platform::PlatformWindowService>(platform.NativeWindowHandle()));
        Software::App::DefaultModules::Register(context);

        // Tracks frame timing for updates and animation
        double previousTime = glfwGetTime();
        context.frame.elapsedSeconds = previousTime;
        context.frame.deltaSeconds = 0.0;
        context.frame.frameIndex = 0;

        // Drives the app until the window closes or the app asks to quit
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

            // Updates state before drawing
            features.Update(context);
            tools.Update(context);

            // Draws any world space content first
            features.DrawWorld(context);
            tools.DrawWorld(context);

            // Draws the UI after world content
            features.DrawUI(context);
            tools.DrawUI(context);

            platform.EndFrame();
        }

        // Clears platform resources before leaving
        platform.Shutdown();
        services.Clear();
        events.Clear();

        return 0;
    }
}
