#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Core/Runtime/Mode.h"

namespace Software::Core::Runtime
{
    /** Registry that manages creation, activation, and routing for tools/modes. */
    class ToolRegistry
    {
        // Functions
    public:
        /** Factory function signature used to construct tool instances. */
        using Factory = std::function<ModePtr()>;

        /** Registers a tool factory with the given name. */
        void Register(std::string name, Factory factory);

        /** Activates the named tool if available. */
        bool Activate(const std::string& name, AppContext& context);

        /** Updates the active tool. */
        void Update(AppContext& context);

        /** Renders world content for the active tool. */
        void DrawWorld(AppContext& context);

        /** Renders UI content for the active tool. */
        void DrawUI(AppContext& context);

        /** Renders details panes for the active tool. */
        void DrawDetails(AppContext& context);

        /** Returns a raw pointer to the active tool instance. */
        Mode* Active() const;

        /** Returns the name of the active tool. */
        const std::string& ActiveName() const;

        /** Listing information for registered tools. */
        struct Listing
        {
            /** Display name for the tool. */
            std::string name;

            /** Whether the tool is currently active. */
            bool bActive = false;
        };

        /** Returns a copy of the tool listings. */
        std::vector<Listing> List() const;

        /** Routes mouse button events to the active tool. */
        InputResult RouteMouseButton(const MouseButtonEvent& event, AppContext& context);

        /** Routes mouse move events to the active tool. */
        InputResult RouteMouseMove(const MouseMoveEvent& event, AppContext& context);

        /** Routes scroll events to the active tool. */
        InputResult RouteScroll(const ScrollEvent& event, AppContext& context);

        /** Routes keyboard events to the active tool. */
        InputResult RouteKey(const KeyEvent& event, AppContext& context);

        // Variables
    private:
        /** Internal entry describing a tool factory and instance. */
        struct Entry
        {
            /** Display name for the tool. */
            std::string name;

            /** Factory used to construct tool instances. */
            Factory factory;

            /** Cached instance if already created. */
            ModePtr instance;
        };

        /** Registered tool entries. */
        std::vector<Entry> m_entries;

        /** Pointer to the active entry, if any. */
        Entry* m_activeEntry = nullptr;

        /** Cached active name string. */
        std::string m_activeName;
    };
}
