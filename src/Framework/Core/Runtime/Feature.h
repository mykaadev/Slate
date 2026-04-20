#pragma once

#include <memory>

#include "Core/Runtime/AppContext.h"
#include "Core/Runtime/InputEvents.h"

namespace Software::Core::Runtime
{
    /** Feature interface providing lifecycle hooks for shared systems. */
    class Feature
    {
    // Functions
    public:
    
        /** Virtual destructor for safe polymorphic cleanup. */
        virtual ~Feature() = default;

        /** Called when the feature is first registered with the framework. */
        virtual void OnRegister(AppContext&)
        {
        }

        /** Called when the feature is enabled prior to update and render loops. */
        virtual void OnEnable(AppContext&)
        {
        }

        /** Called when the feature is disabled to release transient state. */
        virtual void OnDisable(AppContext&)
        {
        }

        /** Called every frame to update feature logic. */
        virtual void Update(AppContext&)
        {
        }

        /** Called every frame to render world content. */
        virtual void DrawWorld(AppContext&)
        {
        }

        /** Called every frame to render UI overlays. */
        virtual void DrawUI(AppContext&)
        {
        }

        /** Called to render details panels or inspector content. */
        virtual void DrawDetails(AppContext&)
        {
        }

        /** Handles mouse button events routed through the input system. */
        virtual InputResult OnMouseButton(const MouseButtonEvent&, AppContext&)
        {
            return InputResult::Ignored;
        }

        /** Handles mouse move events routed through the input system. */
        virtual InputResult OnMouseMove(const MouseMoveEvent&, AppContext&)
        {
            return InputResult::Ignored;
        }

        /** Handles scroll wheel events routed through the input system. */
        virtual InputResult OnScroll(const ScrollEvent&, AppContext&)
        {
            return InputResult::Ignored;
        }

        /** Handles key input events routed through the input system. */
        virtual InputResult OnKey(const KeyEvent&, AppContext&)
        {
            return InputResult::Ignored;
        }
    };

    using FeaturePtr = std::shared_ptr<Feature>;
}
