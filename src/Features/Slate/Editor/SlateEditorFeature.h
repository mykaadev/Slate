#pragma once

#include "App/Slate/Editor/SlateEditorContext.h"
#include "Core/Runtime/Feature.h"

#include <memory>

namespace Software::Features::SlateEditor
{
    // Shares editor services with the rest of the app
    class SlateEditorFeature final : public Software::Core::Runtime::Feature
    {
    public:
        // Registers the editor state when the feature turns on
        void OnEnable(Software::Core::Runtime::AppContext& context) override;

    private:
        // Keeps one editor state alive across modes
        std::shared_ptr<Software::Slate::SlateEditorContext> m_state =
            std::make_shared<Software::Slate::SlateEditorContext>();
    };
}
