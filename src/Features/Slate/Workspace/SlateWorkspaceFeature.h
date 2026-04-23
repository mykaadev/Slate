#pragma once

#include "App/Slate/State/SlateWorkspaceContext.h"
#include "Core/Runtime/Feature.h"

#include <memory>

namespace Software::Features::SlateWorkspace
{
    // Shares workspace services and updates them each frame
    class SlateWorkspaceFeature final : public Software::Core::Runtime::Feature
    {
    public:
        // Registers workspace services before modes start
        void OnEnable(Software::Core::Runtime::AppContext& context) override;
        // Advances background workspace work each frame
        void Update(Software::Core::Runtime::AppContext& context) override;

    private:
        // Keeps one workspace state alive across the app
        std::shared_ptr<Software::Slate::SlateWorkspaceContext> m_state =
            std::make_shared<Software::Slate::SlateWorkspaceContext>();
    };
}
