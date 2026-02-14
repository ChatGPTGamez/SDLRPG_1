#pragma once
#include <stdbool.h>

typedef struct SDL_Renderer SDL_Renderer;
typedef struct PlatformApp PlatformApp;
typedef struct LayeredMap LayeredMap;

typedef struct InteractionSystem
{
    bool prompt_visible;
    int  prompt_tx;
    int  prompt_ty;
    int  prompt_id;

    bool dialog_open;
    char dialog_text[512];
} InteractionSystem;

void Interaction_Init(InteractionSystem* is);
bool Interaction_IsDialogOpen(const InteractionSystem* is);

// Update: finds nearby interactables and opens/closes dialog with E/Esc.
void Interaction_Update(InteractionSystem* is,
                        const PlatformApp* app,
                        const LayeredMap* map,
                        float player_x, float player_y);

// Render UI overlay (prompt + message box) in screen-space.
void Interaction_RenderHUD(InteractionSystem* is,
                           SDL_Renderer* r,
                           int screen_w, int screen_h);
