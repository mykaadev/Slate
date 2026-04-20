#pragma once

#include <memory>

#include "Core/Runtime/AppContext.h"

namespace Software::Core::UI
{
    /** Interface representing a UI panel renderable through ImGui. */
    class IPanel
    {
        // Functions
    public:
        /** Virtual destructor for safe polymorphic cleanup. */
        virtual ~IPanel() = default;

        /** Unique identifier used for window naming. */
        virtual const char* Id() const = 0;

        /** Draws the panel content. */
        virtual void Draw(Software::Core::Runtime::AppContext& context) = 0;
    };

    /** Convenience alias for owning panel pointers. */
    using PanelPtr = std::unique_ptr<IPanel>;
}
