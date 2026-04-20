#pragma once

#include "Core/Runtime/Feature.h"

namespace Software::Features::Sample
{
    /** Example feature showcasing simple counter UI elements. */
    class SampleFeature final : public Software::Core::Runtime::Feature
    {
    // Functions
    public:
    
        /** Enables the sample feature and seeds state. */
        void OnEnable(Software::Core::Runtime::AppContext& context) override;

        /** Draws the sample feature UI content. */
        void DrawUI(Software::Core::Runtime::AppContext& context) override;

        /** Draws details and state visualization for the sample feature. */
        void DrawDetails(Software::Core::Runtime::AppContext& context) override;

    // Variables
    private:

        /** Tracks the current counter value. */
        int m_counter = 0;

        /** Whether to show the sample window. */
        bool bShowWindow = true;
    };
}
