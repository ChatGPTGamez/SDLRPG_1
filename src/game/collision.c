// src/game/collision.c
#include "game/collision.h"
#include "world/layered_map.h"

static int i_floor_div(float v, int tile_size)
{
    return (int)(v / (float)tile_size); // world coords are non-negative in your engine
}

static bool rect_collides_tiles(const LayeredMap* m, const SDL_FRect* r)
{
    if (!m || !r) return true;
    const int ts = m->tile_size;
    if (ts <= 0) return true;

    // Sample covered tile range
    const float eps = 0.001f;
    const int tx0 = i_floor_div(r->x, ts);
    const int ty0 = i_floor_div(r->y, ts);
    const int tx1 = i_floor_div(r->x + r->w - eps, ts);
    const int ty1 = i_floor_div(r->y + r->h - eps, ts);

    for (int ty = ty0; ty <= ty1; ++ty)
    {
        for (int tx = tx0; tx <= tx1; ++tx)
        {
            if (LayeredMap_Solid(m, tx, ty))
                return true;
        }
    }
    return false;
}

static void resolve_x(const LayeredMap* m, SDL_FRect* r, float dx)
{
    if (!m || !r) return;
    const int ts = m->tile_size;
    if (ts <= 0) return;

    if (dx > 0.0f)
    {
        // Moving right: clamp to left edge of the first solid tile we hit on the right side
        const float eps = 0.001f;
        const int tx = i_floor_div(r->x + r->w - eps, ts);

        const int ty0 = i_floor_div(r->y, ts);
        const int ty1 = i_floor_div(r->y + r->h - eps, ts);

        for (int ty = ty0; ty <= ty1; ++ty)
        {
            if (LayeredMap_Solid(m, tx, ty))
            {
                r->x = (float)(tx * ts) - r->w; // snap flush
                return;
            }
        }
    }
    else if (dx < 0.0f)
    {
        // Moving left: clamp to right edge of the first solid tile we hit on the left side
        const int tx = i_floor_div(r->x, ts);

        const float eps = 0.001f;
        const int ty0 = i_floor_div(r->y, ts);
        const int ty1 = i_floor_div(r->y + r->h - eps, ts);

        for (int ty = ty0; ty <= ty1; ++ty)
        {
            if (LayeredMap_Solid(m, tx, ty))
            {
                r->x = (float)((tx + 1) * ts); // snap flush
                return;
            }
        }
    }
}

static void resolve_y(const LayeredMap* m, SDL_FRect* r, float dy)
{
    if (!m || !r) return;
    const int ts = m->tile_size;
    if (ts <= 0) return;

    if (dy > 0.0f)
    {
        // Moving down
        const float eps = 0.001f;
        const int ty = i_floor_div(r->y + r->h - eps, ts);

        const int tx0 = i_floor_div(r->x, ts);
        const int tx1 = i_floor_div(r->x + r->w - eps, ts);

        for (int tx = tx0; tx <= tx1; ++tx)
        {
            if (LayeredMap_Solid(m, tx, ty))
            {
                r->y = (float)(ty * ts) - r->h;
                return;
            }
        }
    }
    else if (dy < 0.0f)
    {
        // Moving up
        const int ty = i_floor_div(r->y, ts);

        const float eps = 0.001f;
        const int tx0 = i_floor_div(r->x, ts);
        const int tx1 = i_floor_div(r->x + r->w - eps, ts);

        for (int tx = tx0; tx <= tx1; ++tx)
        {
            if (LayeredMap_Solid(m, tx, ty))
            {
                r->y = (float)((ty + 1) * ts);
                return;
            }
        }
    }
}

SDL_FRect Collision_PlayerFeetHitbox(float player_x, float player_y, int tile_size)
{
    // Feet box: centered-ish, lower portion of the sprite footprint
    // Tune these if you change player art dimensions later.
    const float ts = (float)tile_size;

    SDL_FRect r;
    r.x = player_x + ts * 0.25f;
    r.y = player_y + ts * 0.55f;
    r.w = ts * 0.50f;
    r.h = ts * 0.35f;
    return r;
}

void Collision_MoveBox_Tiles(const LayeredMap* m, SDL_FRect* box, float dx, float dy)
{
    if (!m || !box) return;

    // Move X then resolve
    box->x += dx;
    if (rect_collides_tiles(m, box))
    {
        resolve_x(m, box, dx);
    }

    // Move Y then resolve
    box->y += dy;
    if (rect_collides_tiles(m, box))
    {
        resolve_y(m, box, dy);
    }
}
