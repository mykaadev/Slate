#pragma once

#include "Core/UI/IPanel.h"

namespace Software::UI::Details
{
    class DetailsPanel : public Software::Core::UI::IPanel
    {
    public:
        const char* Id() const override;
        void Draw(Software::Core::Runtime::AppContext& context) override;
    };
}
