// src/game/interaction.c
#include "interaction.h"
#include "game.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "../world/layered_map.h"
#include "../ui/message_box.h"

static void probe_front_world(const Game* g, float* out_wx, float* out_wy)
{
    const int ts = g->map->tile_size;

    // Check from player center-ish toward facing direction by 1 tile
    const float px = g->player_x + 12.0f; // half of PLAYER_W (24)
    const float py = g->player_y + 12.0f; // half of PLAYER_H

    float wx = px;
    float wy = py;

    switch (g->facing)
    {
        case FACE_UP:    wy -= (float)ts; break;
        case FACE_DOWN:  wy += (float)ts; break;
        case FACE_LEFT:  wx -= (float)ts; break;
        case FACE_RIGHT: wx += (float)ts; break;
        default: break;
    }

    *out_wx = wx;
    *out_wy = wy;
}

InteractProbe Interaction_ProbeFront(const Game* g)
{
    InteractProbe p;
    memset(&p, 0, sizeof(p));

    if (!g || !g->map) return p;

    float wx, wy;
    probe_front_world(g, &wx, &wy);

    const int ts = g->map->tile_size;
    const int tx = (int)(wx / (float)ts);
    const int ty = (int)(wy / (float)ts);

    p.tx = tx;
    p.ty = ty;
    p.valid = (tx >= 0 && ty >= 0 && tx < g->map->width && ty < g->map->height);
    p.id = p.valid ? LayeredMap_Interact(g->map, tx, ty) : 0;

    return p;
}

static const char* interact_message_for_id(int id)
{
    switch (id)
    {
        case 1: return "Sign: Hello traveler.";
        case 2: return "NPC: Nice weather today.";
        case 3: return "Chest: It's locked.";
        default: return NULL;
    }
}

bool Interaction_Try(Game* g)
{
    if (!g || !g->map) return false;

    // If a message is open, pressing E closes it (simple UX)
    if (g->msg.visible)
    {
        MessageBox_Hide(&g->msg);
        return true;
    }

    InteractProbe p = Interaction_ProbeFront(g);
    if (!p.valid || p.id == 0) return false;

    const char* msg = interact_message_for_id(p.id);
    if (!msg) msg = "Something is here, but it's not implemented yet.";

    SDL_Log("INTERACT id=%d at (%d,%d): %s", p.id, p.tx, p.ty, msg);
    MessageBox_Show(&g->msg, msg);
    return true;
}
