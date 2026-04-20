#pragma once

namespace Software::Core::App
{
    /** Application entry point responsible for running the main loop. */
    class Application
    {
    // Functions
    public:

        /** Runs the platform loop and dispatches framework updates. */
        int Run();
    };
}
