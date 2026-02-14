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

    if (tile_size <= 0)
    {
        SDL_Log("Tiles_Load: invalid tile_size=%d", tile_size);
        return false;
    }

    g_tiles_cols = (int)(tw / (float)tile_size);
    if (g_tiles_cols <= 0)
    {
        SDL_Log("Tiles_Load: tileset cols computed as %d (tex_w=%.1f, tile=%d)", g_tiles_cols, tw, tile_size);
        return false;
    }

    SDL_Log("Tileset loaded: %s cols=%d", path, g_tiles_cols);
    return true;
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

    // Clamp to world (handles world smaller than screen too)
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

    // Facing
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

    // Normalize so diagonals aren't faster
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

    // Tile collision + sliding
    Collision_MoveBox_Tiles(g->map, &feet, dx, dy);

    // Convert feet rect back to entity origin using the same ratios
    p->x = feet.x - (float)ts * p->feet_off_x;
    p->y = feet.y - (float)ts * p->feet_off_y;

    // Block against other solid entities (NPC/door)
    EntitySystem_ResolveSolids(&g->ents, p, ts);

    // Keep legacy fields synced
    g->player_x = p->x;
    g->player_y = p->y;
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

    if (!LayeredMap_LoadFromFile(g->map, "assets/maps/test.map3"))
    {
        SDL_Log("Game_Init: LayeredMap_LoadFromFile failed");
        return false;
    }

    // Default spawn if unset
    if (g->player_x == 0.0f && g->player_y == 0.0f)
    {
        g->player_x = (float)g->map->tile_size * 4.0f;
        g->player_y = (float)g->map->tile_size * 4.0f;
    }

    Interaction_Init(&g->interact);
    EntitySystem_Init(&g->ents);

    // Spawn Player entity
    Entity* p = EntitySystem_Spawn(&g->ents, ENT_PLAYER, g->player_x, g->player_y);
    g->player_eid = p ? p->id : 0;

    // Make placeholder visuals match tile size (keeps hitbox + visuals sane)
    const float ts = (float)g->map->tile_size;
    if (p)
    {
        p->w = ts;
        p->h = ts;
        SDL_strlcpy(p->name, "Player", sizeof(p->name));
    }

    // Spawn test NPC + Door
    Entity* npc = EntitySystem_Spawn(&g->ents, ENT_NPC, g->player_x + ts * 2.0f, g->player_y + ts * 1.0f);
    if (npc)
    {
        npc->w = ts;
        npc->h = ts;
        SDL_strlcpy(npc->name, "NPC", sizeof(npc->name));
    }

    Entity* door = EntitySystem_Spawn(&g->ents, ENT_DOOR, g->player_x + ts * 4.0f, g->player_y + ts * 2.0f);
    if (door)
    {
        door->w = ts;
        door->h = ts;
        SDL_strlcpy(door->name, "Door", sizeof(door->name));
    }

    SDL_Log("Game_Init OK");
    return true;
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

    // IMPORTANT: must be edge-trigger (Pressed), not Down
    if (Input_Pressed(&app->input, SDL_SCANCODE_F1))
        g->debug_collision = !g->debug_collision;

    // Sync from entity before interaction checks
    Entity* p = EntitySystem_FindById(&g->ents, g->player_eid);
    if (p)
    {
        g->player_x = p->x;
        g->player_y = p->y;
    }

    // Existing tile-based interaction
    Interaction_Update(&g->interact, app, g->map, g->player_x, g->player_y);

    if (!Interaction_IsDialogOpen(&g->interact))
        Move_Player_Entity(g, app, dt);
}

void Game_Render(Game* g, PlatformApp* app)
{
    if (!g || !app || !g->map) return;

    SDL_Renderer* r = app->renderer;
    const LayeredMap* m = g->map;
    const int ts = m->tile_size;

    // 1) ALWAYS CLEAR. This fixes "stuck between debug and not".
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    if (!g_tiles_tex)
        (void)Tiles_Load(r, "assets/tiles/tileset.png", ts);

    // Camera focuses player entity
    Entity* pEnt = EntitySystem_FindById(&g->ents, g->player_eid);
    if (pEnt)
    {
        g->player_x = pEnt->x;
        g->player_y = pEnt->y;
    }

    float cam_x = 0.0f, cam_y = 0.0f;
    Calc_Camera(m, g->player_x, g->player_y, app->win_w, app->win_h, &cam_x, &cam_y);

    // Center world if it's smaller than the window
    const float world_w = (float)(m->width * ts);
    const float world_h = (float)(m->height * ts);

    float off_x = 0.0f, off_y = 0.0f;
    if (world_w < (float)app->win_w) off_x = ((float)app->win_w - world_w) * 0.5f;
    if (world_h < (float)app->win_h) off_y = ((float)app->win_h - world_h) * 0.5f;

    // Visible tile window
    int tx0 = (int)floorf(cam_x / (float)ts);
    int ty0 = (int)floorf(cam_y / (float)ts);
    int tx1 = (int)ceilf((cam_x + (float)app->win_w) / (float)ts) + 1;
    int ty1 = (int)ceilf((cam_y + (float)app->win_h) / (float)ts) + 1;

    // 2) CLAMP to map bounds. This fixes phantom tiles/collision.
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 > m->width)  tx1 = m->width;
    if (ty1 > m->height) ty1 = m->height;

    // ---- ground
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

    // ---- deco
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

    // ---- coll placeholder (ONLY inside map, ONLY if no deco tile there)
    // This makes your coll-only structures visible without contaminating the whole scene.
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

    // ---- debug collision overlay (ONLY when enabled)
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

    // ---- entities (Y-sort)
    int ids[ENTITY_MAX];
    const int n = EntitySystem_BuildRenderListY(&g->ents, ids, ENTITY_MAX);

    for (int i = 0; i < n; ++i)
    {
        Entity* e = EntitySystem_FindById(&g->ents, ids[i]);
        if (!e) continue;

        SDL_FRect vr = Entity_VisualRect(e);
        vr.x = vr.x - cam_x + off_x;
        vr.y = vr.y - cam_y + off_y;

        if (e->type == ENT_PLAYER)
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        else if (e->type == ENT_NPC)
            SDL_SetRenderDrawColor(r, 255, 200, 0, 255);
        else if (e->type == ENT_DOOR)
            SDL_SetRenderDrawColor(r, 80, 160, 255, 255);
        else
            SDL_SetRenderDrawColor(r, 200, 200, 200, 255);

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

    // ---- HUD
    Interaction_RenderHUD(&g->interact, r, app->win_w, app->win_h);

    // NOTE: Present is handled by your PlatformApp loop, not here.
}
