// src/platform/platform_app.c
#include "platform_app.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

static void wsl_env_fixup(void)
{
    const char* disp = getenv("DISPLAY");
    const char* way  = getenv("WAYLAND_DISPLAY");

    // Prefer X11 when DISPLAY exists (common WSL+Xserver).
    if (disp && disp[0])
    {
        const char* vd = getenv("SDL_VIDEODRIVER");
        if (!vd || !vd[0])
            setenv("SDL_VIDEODRIVER", "x11", 1);
    }
    else if (way && way[0])
    {
        const char* vd = getenv("SDL_VIDEODRIVER");
        if (!vd || !vd[0])
            setenv("SDL_VIDEODRIVER", "wayland", 1);
    }

    // Avoid warning spam if missing.
    const char* xdg = getenv("XDG_RUNTIME_DIR");
    if (!xdg || !xdg[0])
        setenv("XDG_RUNTIME_DIR", "/run/user/1000", 0);
}

bool PlatformApp_Init(PlatformApp* app, const char* title, int w, int h)
{
    if (!app) return false;
    memset(app, 0, sizeof(*app));

    wsl_env_fixup();

    SDL_Log("ENV DISPLAY=%s", getenv("DISPLAY") ? getenv("DISPLAY") : "(null)");
    SDL_Log("ENV WAYLAND_DISPLAY=%s", getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "(null)");
    SDL_Log("ENV XDG_RUNTIME_DIR=%s", getenv("XDG_RUNTIME_DIR") ? getenv("XDG_RUNTIME_DIR") : "(null)");
    SDL_Log("ENV SDL_VIDEODRIVER=%s", getenv("SDL_VIDEODRIVER") ? getenv("SDL_VIDEODRIVER") : "(null)");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    SDL_Log("SDL_Init OK");
    SDL_Log("Video driver: %s", SDL_GetCurrentVideoDriver());

    app->window = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE);
    if (!app->window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    SDL_ShowWindow(app->window);

    // SDL3: SDL_CreateRenderer(window, name)
    app->renderer = SDL_CreateRenderer(app->window, NULL);
    if (!app->renderer)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(app->window);
        app->window = NULL;
        SDL_Quit();
        return false;
    }

    SDL_SetRenderVSync(app->renderer, 1);

    SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);

    app->running = true;
    app->has_focus = true; // WSL sometimes doesn't deliver initial focus event

    // No PlatformInput_Init exists in your codebase; zeroed by memset() already.
    // (PlatformInput_BeginFrame will handle per-frame resets.)

    SDL_Log("PlatformApp_Init OK");
    return true;
}

void PlatformApp_Shutdown(PlatformApp* app)
{
    if (!app) return;

    if (app->renderer)
    {
        SDL_DestroyRenderer(app->renderer);
        app->renderer = NULL;
    }

    if (app->window)
    {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

void PlatformApp_PumpEvents(PlatformApp* app)
{
    if (!app) return;

    // reset key_pressed / key_released each frame
    PlatformInput_BeginFrame(&app->input);

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_EVENT_QUIT:
                app->running = false;
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                app->has_focus = true;
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                app->has_focus = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                SDL_GetWindowSize(app->window, &app->win_w, &app->win_h);
                break;

            default:
                break;
        }

        PlatformInput_HandleEvent(&app->input, &e);
    }
}

// Engine_Tick expects this symbol.
// We'll keep it as a clear stage + optional per-frame upkeep hook.
void PlatformApp_BeginFrame(PlatformApp* app)
{
    if (!app || !app->renderer) return;

    SDL_SetRenderDrawColor(app->renderer, 20, 20, 24, 255);
    SDL_RenderClear(app->renderer);
}

// Compatibility wrappers (some code may still call these)
void PlatformApp_RenderBegin(PlatformApp* app) { PlatformApp_BeginFrame(app); }

void PlatformApp_RenderEnd(PlatformApp* app)
{
    if (!app || !app->renderer) return;
    SDL_RenderPresent(app->renderer);
}
