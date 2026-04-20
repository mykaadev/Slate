#include "PanelHost.h"

namespace Software::Core::UI
{
    void PanelHost::Register(PanelPtr panel)
    {
        m_panels.push_back(std::move(panel));
    }

    void PanelHost::Draw(Software::Core::Runtime::AppContext& context)
    {
        for (auto& panel : m_panels)
        {
            panel->Draw(context);
        }
    }
}
