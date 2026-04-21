#pragma once

#include "App/Slate/SlateWorkspaceContext.h"
#include "Core/Runtime/Feature.h"

#include <memory>

namespace Software::Features::SlateWorkspace
{
    class SlateWorkspaceFeature final : public Software::Core::Runtime::Feature
    {
    public:
        void OnEnable(Software::Core::Runtime::AppContext& context) override;
        void Update(Software::Core::Runtime::AppContext& context) override;

    private:
        std::shared_ptr<Software::Slate::SlateWorkspaceContext> m_state =
            std::make_shared<Software::Slate::SlateWorkspaceContext>();
    };
}
