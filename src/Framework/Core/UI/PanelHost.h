#pragma once

#include <vector>

#include "Core/UI/IPanel.h"

namespace Software::Core::UI
{
    /** Hosts and renders a collection of UI panels. */
    class PanelHost
    {
        // Functions
    public:
        /** Registers a panel for later drawing. */
        void Register(PanelPtr panel);

        /** Draws all registered panels. */
        void Draw(Software::Core::Runtime::AppContext& context);

        // Variables
    private:
        /** Stored panel instances. */
        std::vector<PanelPtr> m_panels;
    };
}
