#include "PlatformLayer.h"

#include "Core/Runtime/AppConfig.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace Software::Platform
{
    bool PlatformLayer::Initialize()
    {
        if (!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

        m_window = glfwCreateWindow(1280, 720, SOFTWARE_APP_NAME, nullptr, nullptr);
        if (!m_window)
        {
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);



        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        glfwSetWindowUserPointer(m_window, this);
        InstallCallbacks();

        return true;
    }

    void PlatformLayer::Shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        glfwTerminate();
    }

    void PlatformLayer::PollEvents()
    {
        glfwPollEvents();
    }

    void PlatformLayer::BeginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void PlatformLayer::EndFrame()
    {
        ImGui::Render();

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }

    bool PlatformLayer::ShouldClose() const
    {
        return m_window == nullptr || glfwWindowShouldClose(m_window);
    }

    void PlatformLayer::SetInputRouter(Software::Core::Runtime::InputRouter* router)
    {
        m_router = router;
    }

    void PlatformLayer::InstallCallbacks()
    {
        m_prevCursorPos = glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
            auto* self = static_cast<PlatformLayer*>(glfwGetWindowUserPointer(window));
            if (self && self->m_prevCursorPos)
            {
                self->m_prevCursorPos(window, x, y);
            }
            if (self)
            {
                self->HandleCursorPos(x, y);
            }
        });

        m_prevMouseButton = glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
            auto* self = static_cast<PlatformLayer*>(glfwGetWindowUserPointer(window));
            if (self && self->m_prevMouseButton)
            {
                self->m_prevMouseButton(window, button, action, mods);
            }
            if (self)
            {
                self->HandleMouseButton(button, action, mods);
            }
        });

        m_prevScroll = glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xOffset, double yOffset) {
            auto* self = static_cast<PlatformLayer*>(glfwGetWindowUserPointer(window));
            if (self && self->m_prevScroll)
            {
                self->m_prevScroll(window, xOffset, yOffset);
            }
            if (self)
            {
                self->HandleScroll(xOffset, yOffset);
            }
        });

        m_prevKey = glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto* self = static_cast<PlatformLayer*>(glfwGetWindowUserPointer(window));
            if (self && self->m_prevKey)
            {
                self->m_prevKey(window, key, scancode, action, mods);
            }
            if (self)
            {
                self->HandleKey(key, scancode, action, mods);
            }
        });
    }

    Software::Core::Runtime::MouseButton PlatformLayer::TranslateMouseButton(int button)
    {
        using Software::Core::Runtime::MouseButton;
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_RIGHT:
            return MouseButton::Right;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            return MouseButton::Middle;
        default:
            return MouseButton::Left;
        }
    }

    void PlatformLayer::HandleCursorPos(double x, double y)
    {
        if (!m_router)
        {
            return;
        }

        ImVec2 current(static_cast<float>(x), static_cast<float>(y));
        ImVec2 delta(current.x - m_lastMousePos.x, current.y - m_lastMousePos.y);
        m_lastMousePos = current;

        Software::Core::Runtime::MouseMoveEvent event{};
        event.position = current;
        event.delta = delta;
        m_router->OnMouseMove(event);
    }

    void PlatformLayer::HandleMouseButton(int button, int action, int mods)
    {
        if (!m_router)
        {
            return;
        }

        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(m_window, &x, &y);
        m_lastMousePos = ImVec2(static_cast<float>(x), static_cast<float>(y));

        Software::Core::Runtime::MouseButtonEvent event{};
        event.button = TranslateMouseButton(button);
        event.action = action == GLFW_PRESS ? Software::Core::Runtime::InputAction::Press
                                            : Software::Core::Runtime::InputAction::Release;
        event.position = m_lastMousePos;
        event.mods = mods;
        m_router->OnMouseButton(event);
    }

    void PlatformLayer::HandleScroll(double xOffset, double yOffset)
    {
        if (!m_router)
        {
            return;
        }

        Software::Core::Runtime::ScrollEvent event{};
        event.offset = ImVec2(static_cast<float>(xOffset), static_cast<float>(yOffset));
        event.position = m_lastMousePos;
        m_router->OnScroll(event);
    }

    void PlatformLayer::HandleKey(int key, int scancode, int action, int mods)
    {
        if (!m_router)
        {
            return;
        }

        Software::Core::Runtime::KeyEvent event{};
        event.key = key;
        event.scancode = scancode;
        event.action = action == GLFW_PRESS ? Software::Core::Runtime::InputAction::Press
                                            : Software::Core::Runtime::InputAction::Release;
        event.mods = mods;
        m_router->OnKey(event);
    }
}
