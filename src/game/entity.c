#include "game/entity.h"
#include <string.h>

SDL_FRect Entity_FeetHitbox(const Entity* e, int tile_size)
{
    const float ts = (float)tile_size;

    SDL_FRect r;
    r.x = e->x + e->feet_off_x * ts;
    r.y = e->y + e->feet_off_y * ts;
    r.w = e->feet_w     * ts;
    r.h = e->feet_h     * ts;
    return r;
}

SDL_FRect Entity_VisualRect(const Entity* e)
{
    SDL_FRect r;
    r.x = e->x;
    r.y = e->y;
    r.w = e->w;
    r.h = e->h;
    return r;
}
