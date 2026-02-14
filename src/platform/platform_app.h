// src/platform/platform_app.h
#pragma once
#include <stdbool.h>

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;

#include "platform_input.h"

typedef struct PlatformApp
{
    SDL_Window* window;
    SDL_Renderer* renderer;

    bool running;
    bool has_focus;

    int win_w;
    int win_h;

    PlatformInput input;
} PlatformApp;

bool PlatformApp_Init(PlatformApp* app, const char* title, int w, int h);
void PlatformApp_Shutdown(PlatformApp* app);

// Pump SDL events + update input state (call once per frame)
void PlatformApp_PumpEvents(PlatformApp* app);

// Engine expects this symbol (call once per frame, early)
void PlatformApp_BeginFrame(PlatformApp* app);

// Kept for compatibility with earlier code paths
void PlatformApp_RenderBegin(PlatformApp* app);
void PlatformApp_RenderEnd(PlatformApp* app);
