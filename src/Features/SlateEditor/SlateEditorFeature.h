#pragma once

#include "App/Slate/SlateEditorContext.h"
#include "Core/Runtime/Feature.h"

#include <memory>

namespace Software::Features::SlateEditor
{
    class SlateEditorFeature final : public Software::Core::Runtime::Feature
    {
    public:
        void OnEnable(Software::Core::Runtime::AppContext& context) override;

    private:
        std::shared_ptr<Software::Slate::SlateEditorContext> m_state =
            std::make_shared<Software::Slate::SlateEditorContext>();
    };
}
