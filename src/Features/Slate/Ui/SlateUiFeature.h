#pragma once

#include "App/Slate/State/SlateUiState.h"
#include "Core/Runtime/Feature.h"

#include <memory>

namespace Software::Features::SlateUi
{
    // Shares the UI state used by Slate modes
    class SlateUiFeature final : public Software::Core::Runtime::Feature
    {
    public:
        // Registers the shared UI state
        void OnEnable(Software::Core::Runtime::AppContext& context) override;

    private:
        // Keeps the UI state alive for the whole app
        std::shared_ptr<Software::Slate::SlateUiState> m_state =
            std::make_shared<Software::Slate::SlateUiState>();
    };
}
