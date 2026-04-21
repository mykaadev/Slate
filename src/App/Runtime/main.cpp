#include "App/Application.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

int main()
{
    Software::Core::App::Application app;
    return app.Run();
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main();
}
#endif
