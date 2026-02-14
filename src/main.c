#include <SDL3/SDL.h>
#include <stdio.h>

#include "platform/platform_app.h"
#include "core/engine.h"

int main(void)
{
    PlatformApp app;

    if (!PlatformApp_Init(&app, "Top-Down RPG Engine (SDL3)", 1280, 720))
    {
        printf("PlatformApp_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    Engine engine;
    EngineConfig cfg = {
        .window_w = 1280,
        .window_h = 720,
        .title = "Top-Down RPG Engine (SDL3)",
        .fixed_hz = 60.0,
        .max_frame_time_sec = 0.25
    };

    if (!Engine_Init(&engine, &app, cfg))
    {
        printf("Engine_Init failed\n");
        PlatformApp_Shutdown(&app);
        return 1;
    }

    while (Engine_Tick(&engine))
    {
        // Engine owns per-frame work now.
    }

    Engine_Shutdown(&engine);
    PlatformApp_Shutdown(&app);
    return 0;
}
