#pragma once

#include "Core/Runtime/Module.h"

#include <memory>

namespace Software::Slate
{
    class SlateEditorContext;
}

namespace Software::Slate::Modules
{
    /** Owns editor-domain state and native editor integration. */
    class SlateEditorModule final : public Software::Core::Runtime::IModule
    {
    public:
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        void Register(Software::Core::Runtime::ModuleContext& context) override;

    private:
        std::shared_ptr<Software::Slate::SlateEditorContext> m_editor;
    };
}
