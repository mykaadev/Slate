#pragma once

#include "App/Slate/Core/NavigationController.h"
#include "App/Slate/Core/SlateTypes.h"
#include "Core/Runtime/AppContext.h"

#include <functional>
#include <string>

namespace Software::Slate
{
    class SlateWorkspaceContext;

    /** Callback surface used by the shell/mode that owns workspace side effects. */
    struct WorkspaceSwitcherCallbacks
    {
        /** Opens the selected workspace vault. */
        std::function<void(const WorkspaceVault& vault)> openVault;

        /** Shows workspace setup when the user wants to add/manage vaults. */
        std::function<void()> showSetup;

        /** Emits a shell status message. */
        std::function<void(std::string message)> setStatus;
    };

    /** Owns workspace switcher overlay state, keyboard navigation, and rendering. */
    class SlateWorkspaceSwitcherOverlay
    {
    public:
        /** Opens the switcher and seeds navigation from the active workspace. */
        void Open(const SlateWorkspaceContext& workspace);

        /** Closes the switcher. */
        void Close();

        /** Clears switcher state and closes the overlay. */
        void Reset();

        /** Returns whether the switcher is visible. */
        bool IsOpen() const;

        /** Returns helper text for the global status bar. */
        std::string HelperText() const;

        /** Draws the switcher and handles local keyboard input. */
        void Draw(Software::Core::Runtime::AppContext& context, const WorkspaceSwitcherCallbacks& callbacks);

    private:
        /** Navigation state for workspace rows. */
        NavigationController m_navigation;

        /** True while the switcher is visible. */
        bool m_open = false;
    };
}
