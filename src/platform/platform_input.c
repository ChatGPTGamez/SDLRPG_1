// src/platform/platform_input.c
#include "platform_input.h"
#include <SDL3/SDL.h>

void PlatformInput_BeginFrame(PlatformInput* in)
{
    // Clear edge flags each frame
    SDL_memset(in->key_pressed, 0, sizeof(in->key_pressed));
    SDL_memset(in->key_released, 0, sizeof(in->key_released));
}

void PlatformInput_HandleEvent(PlatformInput* in, const SDL_Event* e)
{
    if (e->type != SDL_EVENT_KEY_DOWN && e->type != SDL_EVENT_KEY_UP)
        return;

    const SDL_Scancode sc = e->key.scancode;

    // Critical: do NOT index arrays unless scancode is in range
    if ((int)sc < 0 || (int)sc >= SDL_SCANCODE_COUNT)
        return;

    const bool is_down = (e->type == SDL_EVENT_KEY_DOWN);
    const bool is_repeat = (e->key.repeat != 0);

    if (is_down)
    {
        if (!in->key_down[sc] && !is_repeat)
            in->key_pressed[sc] = 1;

        in->key_down[sc] = 1;
    }
    else
    {
        if (in->key_down[sc])
            in->key_released[sc] = 1;

        in->key_down[sc] = 0;
    }
}
