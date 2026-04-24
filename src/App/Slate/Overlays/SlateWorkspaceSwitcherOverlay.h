#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>

namespace Software::Slate
{
    class SlateWorkspaceContext;

    // Callback surface used by the shell/mode that owns workspace side effects.
    struct WorkspaceSwitcherCallbacks
    {
        std::function<void(const WorkspaceVault& vault)> openVault;
        std::function<void()> showSetup;
        std::function<void(std::string message)> setStatus;
    };

    // Owns workspace switcher overlay state, keyboard navigation, and rendering.
    class SlateWorkspaceSwitcherOverlay
    {
    public:
        void Open(const SlateWorkspaceContext& workspace);
        void Close();
        void Reset();

        bool IsOpen() const;
        std::string HelperText() const;

        void Draw(Software::Core::Runtime::AppContext& context, const WorkspaceSwitcherCallbacks& callbacks);

    private:
        NavigationController m_navigation;
        bool m_open = false;
    };
}
