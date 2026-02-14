// src/game/game.c
#include "game/game.h"

#include <math.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "platform/platform_app.h"
#include "world/layered_map.h"
#include "game/collision.h"
#include "game/entity.h"
#include "game/entity_system.h"

// ------------------------------------------------------------
// Tileset
// ------------------------------------------------------------
static SDL_Texture* g_tiles_tex = NULL;
static int g_tiles_cols = 0;

static bool Tiles_Load(SDL_Renderer* r, const char* path, int tile_size)
{
    if (g_tiles_tex) return true;

    g_tiles_tex = IMG_LoadTexture(r, path);
    if (!g_tiles_tex)
    {
        SDL_Log("IMG_LoadTexture failed: %s (%s)", path, SDL_GetError());
        return false;
    }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(g_tiles_tex, &tw, &th);

    if (tile_size <= 0) return false;
    g_tiles_cols = (int)(tw / (float)tile_size);

    return (g_tiles_cols > 0);
}

static void Tiles_Unload(void)
{
    if (g_tiles_tex)
    {
        SDL_DestroyTexture(g_tiles_tex);
        g_tiles_tex = NULL;
    }
    g_tiles_cols = 0;
}

static void Draw_Tile(SDL_Renderer* r, int tile_id, int ts, float dx, float dy)
{
    if (!g_tiles_tex) return;
    if (tile_id <= 0) return;

    const int idx = tile_id - 1;
    const int sx = (idx % g_tiles_cols) * ts;
    const int sy = (idx / g_tiles_cols) * ts;

    SDL_FRect src = { (float)sx, (float)sy, (float)ts, (float)ts };
    SDL_FRect dst = { dx, dy, (float)ts, (float)ts };

    SDL_RenderTexture(r, g_tiles_tex, &src, &dst);
}

// ------------------------------------------------------------
// Camera helper
// ------------------------------------------------------------
static void Calc_Camera(const LayeredMap* m,
                        float focus_x, float focus_y,
                        int win_w, int win_h,
                        float* out_cam_x, float* out_cam_y)
{
    const float world_w = (float)(m->width * m->tile_size);
    const float world_h = (float)(m->height * m->tile_size);

    float cx = focus_x - (float)win_w * 0.5f;
    float cy = focus_y - (float)win_h * 0.5f;

    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    const float max_x = world_w - (float)win_w;
    const float max_y = world_h - (float)win_h;

    if (cx > max_x) cx = (max_x < 0) ? 0 : max_x;
    if (cy > max_y) cy = (max_y < 0) ? 0 : max_y;

    *out_cam_x = cx;
    *out_cam_y = cy;
}

// ------------------------------------------------------------
// Player movement: tile collision + sliding + entity solids
// ------------------------------------------------------------
static void Move_Player_Entity(Game* g, const PlatformApp* app, double dt)
{
    Entity* p = EntitySystem_FindById(&g->ents, g->player_eid);
    if (!p || !g->map) return;

    const PlatformInput* in = &app->input;

    float ax = 0.0f, ay = 0.0f;
    if (Input_Down(in, SDL_SCANCODE_A) || Input_Down(in, SDL_SCANCODE_LEFT))  ax -= 1.0f;
    if (Input_Down(in, SDL_SCANCODE_D) || Input_Down(in, SDL_SCANCODE_RIGHT)) ax += 1.0f;
    if (Input_Down(in, SDL_SCANCODE_W) || Input_Down(in, SDL_SCANCODE_UP))    ay -= 1.0f;
    if (Input_Down(in, SDL_SCANCODE_S) || Input_Down(in, SDL_SCANCODE_DOWN))  ay += 1.0f;

    // facing
    if (fabsf(ax) > fabsf(ay))
    {
        if (ax < 0) g->facing = FACE_LEFT;
        else if (ax > 0) g->facing = FACE_RIGHT;
    }
    else
    {
        if (ay < 0) g->facing = FACE_UP;
        else if (ay > 0) g->facing = FACE_DOWN;
    }

    // normalize
    const float len = SDL_sqrtf(ax*ax + ay*ay);
    if (len > 0.0001f)
    {
        ax /= len;
        ay /= len;
    }

    const int ts = g->map->tile_size;

    SDL_FRect feet = Entity_FeetHitbox(p, ts);

    const float step = g->player_speed * (float)dt;
    const float dx = ax * step;
    const float dy = ay * step;

    Collision_MoveBox_Tiles(g->map, &feet, dx, dy);

    // back to origin
    p->x = feet.x - (float)ts * p->feet_off_x;
    p->y = feet.y - (float)ts * p->feet_off_y;

    // collide with other solid entities
    EntitySystem_ResolveSolids(&g->ents, p, ts);

    g->player_x = p->x;
    g->player_y = p->y;
}

// ------------------------------------------------------------
// Door system: load map + respawn entities
// ------------------------------------------------------------
static bool Game_LoadMapAndRespawn(Game* g, const char* map_path, float spawn_x, float spawn_y)
{
    if (!g || !g->map || !map_path) return false;

    // Load map into existing LayeredMap struct
    LayeredMap_Shutdown(g->map);
    if (!LayeredMap_LoadFromFile(g->map, map_path))
    {
        SDL_Log("LoadMap failed: %s", map_path);
        return false;
    }

    SDL_strlcpy(g->current_map, map_path, sizeof(g->current_map));

    // Reset systems that depend on the map
    Interaction_Init(&g->interact);

    // Rebuild entities from scratch (simple + reliable)
    EntitySystem_Init(&g->ents);

    const float ts = (float)g->map->tile_size;

    // Spawn player
    Entity* p = EntitySystem_Spawn(&g->ents, ENT_PLAYER, spawn_x, spawn_y);
    g->player_eid = p ? p->id : 0;

    if (p)
    {
        p->w = ts;
        p->h = ts;
        SDL_strlcpy(p->name, "Player", sizeof(p->name));
        g->player_x = p->x;
        g->player_y = p->y;
    }

    // Spawn one NPC (optional)
    Entity* npc = EntitySystem_Spawn(&g->ents, ENT_NPC, spawn_x + ts * 2.0f, spawn_y + ts * 1.0f);
    if (npc)
    {
        npc->w = ts;
        npc->h = ts;
        SDL_strlcpy(npc->name, "NPC", sizeof(npc->name));
    }

    // Spawn one door that returns to the other map (toggle behavior)
    // Place it 4 tiles right / 2 tiles down from spawn
    Entity* door = EntitySystem_Spawn(&g->ents, ENT_DOOR, spawn_x + ts * 4.0f, spawn_y + ts * 2.0f);
    if (door)
    {
        door->w = ts;
        door->h = ts;
        SDL_strlcpy(door->name, "Door", sizeof(door->name));

        // Determine opposite map for toggle
        const char* a = "assets/maps/test.map3";
        const char* b = "assets/maps/test2.map3";
        const char* target = (SDL_strcmp(g->current_map, a) == 0) ? b : a;

        SDL_strlcpy(door->door_target_map, target, sizeof(door->door_target_map));

        // Spawn the player at a safe position in the other map
        door->door_spawn_x = ts * 4.0f;
        door->door_spawn_y = ts * 4.0f;
    }

    SDL_Log("Loaded map: %s (spawn %.1f,%.1f)", g->current_map, spawn_x, spawn_y);
    return true;
}

static void Door_TryUseNearest(Game* g, PlatformApp* app)
{
    if (!g || !app || !g->map) return;

    Entity* p = EntitySystem_FindById(&g->ents, g->player_eid);
    if (!p) return;

    // Find nearest interactable (NPC or Door), then only act if it's a Door
    Entity* near = EntitySystem_FindNearestInteractable(&g->ents, p, g->map->tile_size, 48.0f);
    if (!near) return;
    if (near->type != ENT_DOOR) return;

    if (near->door_target_map[0] == '\0')
    {
        SDL_Log("Door has no target map set");
        return;
    }

    (void)Game_LoadMapAndRespawn(g, near->door_target_map, near->door_spawn_x, near->door_spawn_y);
}

// ------------------------------------------------------------
// Game lifecycle
// ------------------------------------------------------------
bool Game_Init(Game* g)
{
    if (!g) return false;

    if (g->player_speed <= 0.0f) g->player_speed = 220.0f;
    g->debug_collision = false;

    if (!g->map)
    {
        g->map = (LayeredMap*)SDL_calloc(1, sizeof(LayeredMap));
        if (!g->map) return false;
    }

    // Default first map
    if (g->current_map[0] == '\0')
        SDL_strlcpy(g->current_map, "assets/maps/test.map3", sizeof(g->current_map));

    // Default spawn
    const float spawn_x = 4.0f * 16.0f; // will be corrected after map load using ts
    const float spawn_y = 4.0f * 16.0f;

    // Load map + spawn player/entities
    // We’ll fix spawn to tile_size inside the load function by just using ts*4
    // so pass 0/0 and let it compute after load:
    if (!LayeredMap_LoadFromFile(g->map, g->current_map))
    {
        SDL_Log("Game_Init: LayeredMap_LoadFromFile failed: %s", g->current_map);
        return false;
    }

    // Now call respawn using proper tile size spawn
    const float ts = (float)g->map->tile_size;
    return Game_LoadMapAndRespawn(g, g->current_map, ts * 4.0f, ts * 4.0f);
}

void Game_Shutdown(Game* g)
{
    if (!g) return;

    Tiles_Unload();

    if (g->map)
    {
        LayeredMap_Shutdown(g->map);
        SDL_free(g->map);
        g->map = NULL;
    }

    g->player_eid = 0;
}

void Game_FixedUpdate(Game* g, PlatformApp* app, double dt)
{
    if (!g || !app || !g->map) return;

    if (Input_Pressed(&app->input, SDL_SCANCODE_F1))
        g->debug_collision = !g->debug_collision;

    // Keep legacy synced
    Entity* p = EntitySystem_FindById(&g->ents, g->player_eid);
    if (p)
    {
        g->player_x = p->x;
        g->player_y = p->y;
    }

    // Tile-based interaction (keeps your “Press E” prompt logic)
    Interaction_Update(&g->interact, app, g->map, g->player_x, g->player_y);

    // E-to-use door (entity-based)
    if (Input_Pressed(&app->input, SDL_SCANCODE_E))
        Door_TryUseNearest(g, app);

    if (!Interaction_IsDialogOpen(&g->interact))
        Move_Player_Entity(g, app, dt);
}

void Game_Render(Game* g, PlatformApp* app)
{
    if (!g || !app || !g->map) return;

    SDL_Renderer* r = app->renderer;
    const LayeredMap* m = g->map;
    const int ts = m->tile_size;

    // Clear every frame (prevents “stuck debug” artifacts)
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    if (!g_tiles_tex)
        (void)Tiles_Load(r, "assets/tiles/tileset.png", ts);

    // Focus player
    Entity* pEnt = EntitySystem_FindById(&g->ents, g->player_eid);
    if (pEnt)
    {
        g->player_x = pEnt->x;
        g->player_y = pEnt->y;
    }

    float cam_x = 0.0f, cam_y = 0.0f;
    Calc_Camera(m, g->player_x, g->player_y, app->win_w, app->win_h, &cam_x, &cam_y);

    // Center small maps
    const float world_w = (float)(m->width * ts);
    const float world_h = (float)(m->height * ts);
    float off_x = 0.0f, off_y = 0.0f;
    if (world_w < (float)app->win_w) off_x = ((float)app->win_w - world_w) * 0.5f;
    if (world_h < (float)app->win_h) off_y = ((float)app->win_h - world_h) * 0.5f;

    int tx0 = (int)floorf(cam_x / (float)ts);
    int ty0 = (int)floorf(cam_y / (float)ts);
    int tx1 = (int)ceilf((cam_x + (float)app->win_w) / (float)ts) + 1;
    int ty1 = (int)ceilf((cam_y + (float)app->win_h) / (float)ts) + 1;

    // Clamp to map bounds (no phantom tiles)
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 > m->width)  tx1 = m->width;
    if (ty1 > m->height) ty1 = m->height;

    // Ground
    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int gid = LayeredMap_Ground(m, tx, ty);
            const float dx = (float)(tx * ts) - cam_x + off_x;
            const float dy = (float)(ty * ts) - cam_y + off_y;
            Draw_Tile(r, gid, ts, dx, dy);
        }
    }

    // Deco
    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int did = LayeredMap_Deco(m, tx, ty);
            const float dx = (float)(tx * ts) - cam_x + off_x;
            const float dy = (float)(ty * ts) - cam_y + off_y;
            Draw_Tile(r, did, ts, dx, dy);
        }
    }

    // Coll placeholder (coll is 0/1 only, so give it a visible wall)
    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            if (!LayeredMap_Solid(m, tx, ty)) continue;
            if (LayeredMap_Deco(m, tx, ty) != 0) continue;

            const float dx = (float)(tx * ts) - cam_x + off_x;
            const float dy = (float)(ty * ts) - cam_y + off_y;

            SDL_FRect rc = { dx, dy, (float)ts, (float)ts };
            SDL_SetRenderDrawColor(r, 70, 70, 90, 255);
            SDL_RenderFillRect(r, &rc);
        }
    }

    // Debug collision overlay
    if (g->debug_collision)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int ty = ty0; ty < ty1; ++ty)
        {
            for (int tx = tx0; tx < tx1; ++tx)
            {
                if (!LayeredMap_Solid(m, tx, ty)) continue;

                const float dx = (float)(tx * ts) - cam_x + off_x;
                const float dy = (float)(ty * ts) - cam_y + off_y;

                SDL_FRect rc = { dx, dy, (float)ts, (float)ts };
                SDL_SetRenderDrawColor(r, 255, 0, 0, 70);
                SDL_RenderFillRect(r, &rc);
            }
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    // Entities (Y-sort)
    int ids[ENTITY_MAX];
    const int n = EntitySystem_BuildRenderListY(&g->ents, ids, ENTITY_MAX);

    for (int i = 0; i < n; ++i)
    {
        Entity* e = EntitySystem_FindById(&g->ents, ids[i]);
        if (!e) continue;

        SDL_FRect vr = Entity_VisualRect(e);
        vr.x = vr.x - cam_x + off_x;
        vr.y = vr.y - cam_y + off_y;

        if (e->type == ENT_PLAYER) SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        else if (e->type == ENT_NPC) SDL_SetRenderDrawColor(r, 255, 200, 0, 255);
        else if (e->type == ENT_DOOR) SDL_SetRenderDrawColor(r, 80, 160, 255, 255);
        else SDL_SetRenderDrawColor(r, 200, 200, 200, 255);

        SDL_RenderFillRect(r, &vr);

        if (g->debug_collision)
        {
            SDL_FRect feet = Entity_FeetHitbox(e, ts);
            feet.x = feet.x - cam_x + off_x;
            feet.y = feet.y - cam_y + off_y;

            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 255, 0, 160);
            SDL_RenderRect(r, &feet);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
    }

    // HUD
    Interaction_RenderHUD(&g->interact, r, app->win_w, app->win_h);
}
