#pragma once

#include <functional>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include "imgui.h"

#include "Core/Runtime/InputRouter.h"

namespace Software::Platform
{
    /** Owns the GLFW window and ImGui frame orchestration. */
    class PlatformLayer
    {
        // Functions
    public:
        /** Initializes GLFW, the window, and ImGui backends. */
        bool Initialize();

        /** Shuts down ImGui backends and destroys the window. */
        void Shutdown();

        /** Polls OS events from the windowing system. */
        void PollEvents();

        /** Begins a new ImGui frame. */
        void BeginFrame();

        /** Ends the current ImGui frame and presents the swap chain. */
        void EndFrame();

        /** Indicates whether the platform window should close. */
        bool ShouldClose() const;

        /** Returns the native platform window handle when available. */
        void* NativeWindowHandle() const;

        /** Assigns the input router used to dispatch events. */
        void SetInputRouter(Software::Core::Runtime::InputRouter* router);

        /** Assigns the callback used to report files dropped onto the window. */
        void SetFileDropCallback(std::function<void(std::vector<std::string>)> callback);

        // Functions
    private:
        /** Installs GLFW callbacks for input handling. */
        void InstallCallbacks();

        /** Translates GLFW mouse button values to framework enums. */
        static Software::Core::Runtime::MouseButton TranslateMouseButton(int button);

        /** Handles cursor position events. */
        void HandleCursorPos(double x, double y);

        /** Handles mouse button events. */
        void HandleMouseButton(int button, int action, int mods);

        /** Handles scroll wheel events. */
        void HandleScroll(double xOffset, double yOffset);

        /** Handles key events. */
        void HandleKey(int key, int scancode, int action, int mods);

        /** Handles files dropped onto the window. */
        void HandleDrop(int count, const char** paths);

        // Variables
    private:
        /** Native GLFW window handle. */
        GLFWwindow* m_window = nullptr;

        /** Pointer to the active input router. */
        Software::Core::Runtime::InputRouter* m_router = nullptr;

        /** Previous cursor position callback installed by the application. */
        GLFWcursorposfun m_prevCursorPos = nullptr;

        /** Previous mouse button callback installed by the application. */
        GLFWmousebuttonfun m_prevMouseButton = nullptr;

        /** Previous scroll callback installed by the application. */
        GLFWscrollfun m_prevScroll = nullptr;

        /** Previous key callback installed by the application. */
        GLFWkeyfun m_prevKey = nullptr;

        /** Cached last mouse position. */
        ImVec2 m_lastMousePos{0.0f, 0.0f};

        /** Optional callback receiving dropped file paths. */
        std::function<void(std::vector<std::string>)> m_dropCallback;
    };
}
