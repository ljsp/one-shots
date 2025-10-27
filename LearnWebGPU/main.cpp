#include "application.h"

#include <iostream>

int main()
{
    Application app;

    if (!app.Initialize())
    {
        return 1;
    }

#ifdef __EMSCRIPTEN__

    auto callback = [](void* arg)
        {
            Application* pApp = reinterpret_cast<Application*>(arg);
            pApp->MainLoop();
        }
    emscripten_set_main_loop_arg(callback, &app, 0, true);

#else // __EMSCRIPTEN
    
    while (app.IsRunning())
    {
        app.MainLoop();
    
    }

#endif // __EMSCRIPTEN__

    app.Terminate();

    return 0;
}