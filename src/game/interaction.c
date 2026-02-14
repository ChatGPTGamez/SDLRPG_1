#include "game/interaction.h"

#include <SDL3/SDL.h>
#include <string.h>
#include <math.h>

#include "platform/platform_app.h"
#include "world/layered_map.h"
#include "ui/message_box.h"
#include "ui/ui_text.h"

static MessageBox g_box;

// Hardcoded text table for now (we’ll data-drive later)
static const char* interact_text(int id)
{
    switch (id)
    {
        case 1: return "Sign: Welcome to the test map.";
        case 2: return "NPC: Nice weather today.";
        case 3: return "Chest: It's empty. For now.";
        default: return "Something interesting is here.";
    }
}

void Interaction_Init(InteractionSystem* is)
{
    if (!is) return;
    memset(is, 0, sizeof(*is));
    memset(&g_box, 0, sizeof(g_box));
}

bool Interaction_IsDialogOpen(const InteractionSystem* is)
{
    return is && is->dialog_open;
}

static bool find_nearby_interact(const LayeredMap* map, int ptx, int pty,
                                int* out_tx, int* out_ty, int* out_id)
{
    // Check current + 4-neighbors (1-tile range)
    static const int off[5][2] = { {0,0},{1,0},{-1,0},{0,1},{0,-1} };
    for (int i = 0; i < 5; ++i)
    {
        const int tx = ptx + off[i][0];
        const int ty = pty + off[i][1];
        const int id = LayeredMap_Interact(map, tx, ty); // <-- YOUR API
        if (id > 0)
        {
            *out_tx = tx;
            *out_ty = ty;
            *out_id = id;
            return true;
        }
    }
    return false;
}

void Interaction_Update(InteractionSystem* is,
                        const PlatformApp* app,
                        const LayeredMap* map,
                        float player_x, float player_y)
{
    if (!is || !app || !map) return;

    const PlatformInput* in = &app->input;

    // If dialog open: close with E or Esc
    if (is->dialog_open)
    {
        if (Input_Pressed(in, SDL_SCANCODE_E) || Input_Pressed(in, SDL_SCANCODE_ESCAPE))
        {
            is->dialog_open = false;
            MessageBox_Close(&g_box);
        }
        return;
    }

    // Convert player world->tile
    const int ts = map->tile_size;
    const int ptx = (int)floorf(player_x / (float)ts);
    const int pty = (int)floorf(player_y / (float)ts);

    int tx=0, ty=0, id=0;
    const bool found = find_nearby_interact(map, ptx, pty, &tx, &ty, &id);

    is->prompt_visible = found;
    is->prompt_tx = tx;
    is->prompt_ty = ty;
    is->prompt_id = id;

    if (found && Input_Pressed(in, SDL_SCANCODE_E))
    {
        const char* t = interact_text(id);

        is->dialog_open = true;
        strncpy(is->dialog_text, t, sizeof(is->dialog_text) - 1);
        is->dialog_text[sizeof(is->dialog_text) - 1] = '\0';

        MessageBox_Open(&g_box, is->dialog_text);

        SDL_Log("INTERACT id=%d at (%d,%d): %s", id, tx, ty, is->dialog_text);
    }
}

void Interaction_RenderHUD(InteractionSystem* is,
                           SDL_Renderer* r,
                           int screen_w, int screen_h)
{
    if (!is || !r) return;

    (void)UIText_Init(r);

    // Prompt
    if (is->prompt_visible && !is->dialog_open)
    {
        const char* prompt = "Press E to interact";
        int tw=0, th=0;
        (void)UIText_MeasureLine(prompt, &tw, &th);

        const float x = (float)screen_w * 0.5f - (float)tw * 0.5f;
        const float y = (float)screen_h - 190.0f;

        UIText_DrawLine(r, x, y, prompt);
    }

    // Dialog box
    if (is->dialog_open)
    {
        MessageBox_Render(&g_box, r, screen_w, screen_h);
    }
}
