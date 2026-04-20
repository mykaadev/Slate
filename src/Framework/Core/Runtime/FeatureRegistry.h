#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Core/Runtime/Feature.h"

namespace Software::Core::Runtime
{
    /** Registry that constructs, enables, and routes events for shared features. */
    class FeatureRegistry
    {
    // Functions
    public:
    
        /** Factory function signature used to construct feature instances. */
        using Factory = std::function<FeaturePtr()>;

        /** Registers a feature factory and invokes registration hooks. */
        void Register(std::string name, Factory factory, AppContext& context);

        /** Enables or disables the named feature. */
        bool SetEnabled(const std::string& name, bool enabled, AppContext& context);

        /** Updates all enabled features. */
        void Update(AppContext& context);

        /** Renders world content for enabled features. */
        void DrawWorld(AppContext& context);

        /** Renders UI content for enabled features. */
        void DrawUI(AppContext& context);

        /** Renders details panes for enabled features. */
        void DrawDetails(AppContext& context);

        /** Renders details for a specific feature name. */
        bool DrawDetailsFor(const std::string& name, AppContext& context);

        /** Listing information for registered features. */
        struct Listing
        {
            /** Display name of the feature. */
            std::string name;

            /** Whether the feature is enabled. */
            bool bEnabled = false;
        };

        /** Returns a copy of the feature listings. */
        std::vector<Listing> List() const;

        /** Returns whether the named feature is enabled. */
        bool IsEnabled(const std::string& name) const;

        template <typename Fn>
        void ForEachEnabled(Fn&& fn)
        {
            for (Entry& entry : m_entries)
            {
                if (entry.bEnabled && entry.instance)
                {
                    fn(*entry.instance);
                }
            }
        }

        /** Routes mouse button input to enabled features. */
        InputResult RouteMouseButton(const MouseButtonEvent& event, AppContext& context);

        /** Routes mouse move input to enabled features. */
        InputResult RouteMouseMove(const MouseMoveEvent& event, AppContext& context);

        /** Routes scroll input to enabled features. */
        InputResult RouteScroll(const ScrollEvent& event, AppContext& context);

        /** Routes keyboard input to enabled features. */
        InputResult RouteKey(const KeyEvent& event, AppContext& context);

    // Variables
    private:

        /** Internal entry describing a feature factory, instance, and state. */
        struct Entry
        {
            /** Display name. */
            std::string name;

            /** Factory used to construct the feature instance. */
            Factory factory;

            /** Instantiated feature instance. */
            FeaturePtr instance;

            /** Whether the feature is enabled. */
            bool bEnabled = false;
        };

        /** Finds an entry by name (mutable). */
        Entry* Find(const std::string& name);

        /** Finds an entry by name (const). */
        const Entry* Find(const std::string& name) const;

        /** Registered features. */
        std::vector<Entry> m_entries;
    };
}
