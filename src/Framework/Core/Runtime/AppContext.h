#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Runtime/EventBus.h"
#include "Core/Runtime/ServiceLocator.h"

namespace Software::Core::Runtime
{
    class ToolRegistry;
    class FeatureRegistry;

    /** Frame timing snapshot. */
    struct FrameInfo
    {
        /** Time since app start in seconds. */
        double elapsedSeconds = 0.0;
        
        /** Delta time of the current frame in seconds. */
        double deltaSeconds = 0.0;

        /** Index of the current frame. */
        std::uint64_t frameIndex = 0;
    };

    /** Lightweight view exposing the application backbone to modes/features/panels. */
    struct AppContext
    {
        /** Shared service locator. */
        ServiceLocator& services;

        /** Registry of available tools. */
        ToolRegistry& tools;

        /** Registry of available features. */
        FeatureRegistry& features;

        /** Event bus used for decoupled messaging. */
        EventBus& events;

        /** Timing data for the frame. */
        FrameInfo frame;

        /** Set by modes/tools when the application should close after the frame. */
        bool quitRequested = false;

        /** File paths dropped onto the native window during the current session. */
        std::vector<std::string>* droppedFiles = nullptr;
    };
}
