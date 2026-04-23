#pragma once

namespace Software::Core::App
{
    // Runs the Slate app loop
    class Application
    {
    public:
        // Builds the runtime and drives each frame
        int Run();
    };
}
