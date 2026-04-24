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
        /** Returns module identity for editor services. */
        Software::Core::Runtime::ModuleDescriptor Describe() const override;
        /** Creates and registers the shared editor context. */
        void Register(Software::Core::Runtime::ModuleContext& context) override;

    private:
        /** Shared editor context owned by the module. */
        std::shared_ptr<Software::Slate::SlateEditorContext> m_editor;
    };
}
