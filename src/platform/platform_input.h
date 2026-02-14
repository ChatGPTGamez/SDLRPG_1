#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct PlatformInput
{
    // Keyboard state
    uint8_t key_down[SDL_SCANCODE_COUNT];
    uint8_t key_pressed[SDL_SCANCODE_COUNT];
    uint8_t key_released[SDL_SCANCODE_COUNT];
} PlatformInput;

void PlatformInput_BeginFrame(PlatformInput* in);

// Feed SDL events into input.
void PlatformInput_HandleEvent(PlatformInput* in, const SDL_Event* e);

static inline bool Input_Down(const PlatformInput* in, SDL_Scancode sc)     { return in->key_down[sc] != 0; }
static inline bool Input_Pressed(const PlatformInput* in, SDL_Scancode sc)  { return in->key_pressed[sc] != 0; }
static inline bool Input_Released(const PlatformInput* in, SDL_Scancode sc) { return in->key_released[sc] != 0; }
