#include "game/entity_system.h"
#include <string.h>
#include <math.h>

static float f_abs(float v) { return v < 0 ? -v : v; }

static bool rects_overlap(SDL_FRect a, SDL_FRect b)
{
    return !(a.x + a.w <= b.x ||
             b.x + b.w <= a.x ||
             a.y + a.h <= b.y ||
             b.y + b.h <= a.y);
}

void EntitySystem_Init(EntitySystem* es)
{
    memset(es, 0, sizeof(*es));
    es->next_id = 1;
}

Entity* EntitySystem_Spawn(EntitySystem* es, EntityType type, float x, float y)
{
    if (!es) return NULL;

    // Find a free slot (reuse dead entities)
    int idx = -1;
    for (int i = 0; i < ENTITY_MAX; ++i)
    {
        if (!es->entities[i].alive)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0) return NULL;

    Entity* e = &es->entities[idx];
    memset(e, 0, sizeof(*e));

    e->alive = true;
    e->id    = es->next_id++;
    e->type  = type;
    e->x     = x;
    e->y     = y;

    // Default sizes (visual only, you can tune)
    e->w = 32.0f;
    e->h = 32.0f;

    // Default feet box ratios (relative to tile size)
    e->feet_off_x = 0.25f;
    e->feet_off_y = 0.55f;
    e->feet_w     = 0.50f;
    e->feet_h     = 0.35f;

    // Defaults by type
    if (type == ENT_PLAYER)
    {
        e->flags = ENT_FLAG_SOLID;
        strncpy(e->name, "Player", sizeof(e->name)-1);
    }
    else if (type == ENT_NPC)
    {
        e->flags = ENT_FLAG_SOLID | ENT_FLAG_INTERACTABLE;
        strncpy(e->name, "NPC", sizeof(e->name)-1);
    }
    else if (type == ENT_DOOR)
    {
        e->flags = ENT_FLAG_SOLID | ENT_FLAG_INTERACTABLE;
        strncpy(e->name, "Door", sizeof(e->name)-1);
        // Doors are usually thin; leave visual size alone for now.
    }

    // Update count (best-effort)
    if (idx + 1 > es->count) es->count = idx + 1;

    return e;
}

Entity* EntitySystem_FindById(EntitySystem* es, int id)
{
    if (!es) return NULL;
    for (int i = 0; i < ENTITY_MAX; ++i)
    {
        Entity* e = &es->entities[i];
        if (e->alive && e->id == id) return e;
    }
    return NULL;
}

// Sort helper (insertion sort; small list)
static void sort_ids_by_y(EntitySystem* es, int* ids, int n)
{
    for (int i = 1; i < n; ++i)
    {
        int key = ids[i];
        Entity* ek = EntitySystem_FindById(es, key);
        float keyy = ek ? ek->y : 0.0f;

        int j = i - 1;
        while (j >= 0)
        {
            Entity* ej = EntitySystem_FindById(es, ids[j]);
            float jy = ej ? ej->y : 0.0f;
            if (jy <= keyy) break;
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key;
    }
}

int EntitySystem_BuildRenderListY(EntitySystem* es, int* out_ids, int max_ids)
{
    if (!es || !out_ids || max_ids <= 0) return 0;

    int n = 0;
    for (int i = 0; i < ENTITY_MAX && n < max_ids; ++i)
    {
        if (es->entities[i].alive)
            out_ids[n++] = es->entities[i].id;
    }

    sort_ids_by_y(es, out_ids, n);
    return n;
}

// Push mover out of a single solid rectangle (minimal axis).
static void push_out(SDL_FRect* mover, SDL_FRect solid)
{
    // Compute overlap depths
    float left   = (mover->x + mover->w) - solid.x;
    float right  = (solid.x + solid.w)  - mover->x;
    float top    = (mover->y + mover->h) - solid.y;
    float bottom = (solid.y + solid.h)  - mover->y;

    // Find minimal translation
    float minx = (left < right) ? left : -right;
    float miny = (top  < bottom) ? top  : -bottom;

    if (f_abs(minx) < f_abs(miny))
        mover->x -= minx;
    else
        mover->y -= miny;
}

void EntitySystem_ResolveSolids(EntitySystem* es, const Entity* mover, int tile_size)
{
    if (!es || !mover) return;

    // We resolve by adjusting the *mover's* world origin based on its feet rect.
    Entity* me = EntitySystem_FindById(es, mover->id);
    if (!me) return;

    SDL_FRect myFeet = Entity_FeetHitbox(me, tile_size);

    for (int i = 0; i < ENTITY_MAX; ++i)
    {
        Entity* o = &es->entities[i];
        if (!o->alive) continue;
        if (o->id == me->id) continue;
        if (!(o->flags & ENT_FLAG_SOLID)) continue;

        SDL_FRect oFeet = Entity_FeetHitbox(o, tile_size);
        if (!rects_overlap(myFeet, oFeet)) continue;

        // Push myFeet out of oFeet
        push_out(&myFeet, oFeet);

        // Convert feet rect back to origin
        const float ts = (float)tile_size;
        me->x = myFeet.x - me->feet_off_x * ts;
        me->y = myFeet.y - me->feet_off_y * ts;

        // Recompute after adjustment
        myFeet = Entity_FeetHitbox(me, tile_size);
    }
}

Entity* EntitySystem_FindNearestInteractable(EntitySystem* es,
                                            const Entity* from,
                                            int tile_size,
                                            float radius_world)
{
    if (!es || !from) return NULL;

    SDL_FRect a = Entity_FeetHitbox(from, tile_size);
    float ax = a.x + a.w * 0.5f;
    float ay = a.y + a.h * 0.5f;

    Entity* best = NULL;
    float best_d2 = radius_world * radius_world;

    for (int i = 0; i < ENTITY_MAX; ++i)
    {
        Entity* e = &es->entities[i];
        if (!e->alive) continue;
        if (!(e->flags & ENT_FLAG_INTERACTABLE)) continue;
        if (e->id == from->id) continue;

        SDL_FRect b = Entity_FeetHitbox(e, tile_size);
        float bx = b.x + b.w * 0.5f;
        float by = b.y + b.h * 0.5f;

        float dx = bx - ax;
        float dy = by - ay;
        float d2 = dx*dx + dy*dy;

        if (d2 < best_d2)
        {
            best_d2 = d2;
            best = e;
        }
    }

    return best;
}
