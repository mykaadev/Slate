#pragma once

#include "App/Slate/SlateUiState.h"
#include "Core/Runtime/Feature.h"

#include <memory>

namespace Software::Features::SlateUi
{
    class SlateUiFeature final : public Software::Core::Runtime::Feature
    {
    public:
        void OnEnable(Software::Core::Runtime::AppContext& context) override;

    private:
        std::shared_ptr<Software::Slate::SlateUiState> m_state =
            std::make_shared<Software::Slate::SlateUiState>();
    };
}
