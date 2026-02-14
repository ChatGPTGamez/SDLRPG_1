// src/game/game.c
#include "game/game.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <math.h>
#include <stdbool.h>

#include "platform/platform_app.h"
#include "world/layered_map.h"
#include "game/interaction.h"
#include "game/collision.h"

// NOTE: Interaction system is kept as a small standalone module for now.
static InteractionSystem s_interact;

// Minimal tileset handling (your repo currently uses raw IMG_LoadTexture for tiles).
static SDL_Texture* g_tiles_tex = NULL;
static int g_tiles_cols = 0;
static int g_tiles_rows = 0;

static bool load_tileset(SDL_Renderer* r, const char* path, int tile_size)
{
    if (g_tiles_tex) return true;

    g_tiles_tex = IMG_LoadTexture(r, path);
    if (!g_tiles_tex)
    {
        SDL_Log("IMG_LoadTexture failed for %s : %s", path, SDL_GetError());
        return false;
    }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(g_tiles_tex, &tw, &th);

    const int tex_w = (int)(tw + 0.5f);
    const int tex_h = (int)(th + 0.5f);

    g_tiles_cols = (tile_size > 0) ? (tex_w / tile_size) : 0;
    g_tiles_rows = (tile_size > 0) ? (tex_h / tile_size) : 0;

    SDL_SetTextureBlendMode(g_tiles_tex, SDL_BLENDMODE_BLEND);

    SDL_Log("Tileset loaded: %s (%dx%d) grid=%dx%d", path, tex_w, tex_h, g_tiles_cols, g_tiles_rows);
    return (g_tiles_cols > 0 && g_tiles_rows > 0);
}

static void unload_tileset(void)
{
    if (g_tiles_tex)
    {
        SDL_DestroyTexture(g_tiles_tex);
        g_tiles_tex = NULL;
    }
    g_tiles_cols = 0;
    g_tiles_rows = 0;
}

static void draw_tile_1based(SDL_Renderer* r, int tile_id_1based, int tile_size, float dst_x, float dst_y)
{
    if (!g_tiles_tex) return;
    if (tile_id_1based <= 0) return;
    if (g_tiles_cols <= 0) return;

    // Your map uses 1-based tile IDs; convert to 0-based index for atlas sampling.
    const int idx = tile_id_1based - 1;
    const int sx = (idx % g_tiles_cols) * tile_size;
    const int sy = (idx / g_tiles_cols) * tile_size;

    SDL_FRect src = { (float)sx, (float)sy, (float)tile_size, (float)tile_size };
    SDL_FRect dst = { dst_x, dst_y, (float)tile_size, (float)tile_size };
    SDL_RenderTexture(r, g_tiles_tex, &src, &dst);
}

// Camera clamp to map bounds.
static void camera_calc(const LayeredMap* m, float px, float py,
                        int screen_w, int screen_h,
                        float* out_cam_x, float* out_cam_y)
{
    const float world_w = (float)(m->width * m->tile_size);
    const float world_h = (float)(m->height * m->tile_size);

    float cx = px - (float)screen_w * 0.5f;
    float cy = py - (float)screen_h * 0.5f;

    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    const float max_x = (world_w - (float)screen_w);
    const float max_y = (world_h - (float)screen_h);

    if (cx > max_x) cx = (max_x < 0) ? 0 : max_x;
    if (cy > max_y) cy = (max_y < 0) ? 0 : max_y;

    *out_cam_x = cx;
    *out_cam_y = cy;
}

// Simple player movement (no collision resolution here — that’s a later step).
// Also updates facing + a simple walk animation state.
static void move_player(Game* g, const PlatformApp* app, double dt)
{
    const PlatformInput* in = &app->input;

    float ax = 0.0f, ay = 0.0f;
    if (Input_Down(in, SDL_SCANCODE_A) || Input_Down(in, SDL_SCANCODE_LEFT))  ax -= 1.0f;
    if (Input_Down(in, SDL_SCANCODE_D) || Input_Down(in, SDL_SCANCODE_RIGHT)) ax += 1.0f;
    if (Input_Down(in, SDL_SCANCODE_W) || Input_Down(in, SDL_SCANCODE_UP))    ay -= 1.0f;
    if (Input_Down(in, SDL_SCANCODE_S) || Input_Down(in, SDL_SCANCODE_DOWN))  ay += 1.0f;

    const float len = SDL_sqrtf(ax*ax + ay*ay);
    if (len > 0.0001f)
    {
        ax /= len;
        ay /= len;
    }

    const LayeredMap* m = g->map;
    const int ts = m->tile_size;

    // Build feet hitbox from current player pos
    SDL_FRect feet = Collision_PlayerFeetHitbox(g->player_x, g->player_y, ts);

    const float step = g->player_speed * (float)dt;
    const float dx = ax * step;
    const float dy = ay * step;

    // Move with collision + sliding
    Collision_MoveBox_Tiles(m, &feet, dx, dy);

    // Convert back to player origin so the rest of your engine stays the same
    g->player_x = feet.x - (float)ts * 0.25f;
    g->player_y = feet.y - (float)ts * 0.55f;
}


static bool load_player_sprite(Game* g, SDL_Renderer* r)
{
    if (g->player_tex) return true;

    g->player_tex = IMG_LoadTexture(r, "assets/sprites/player.png");
    if (!g->player_tex)
    {
        SDL_Log("IMG_LoadTexture failed for player sprite: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureBlendMode(g->player_tex, SDL_BLENDMODE_BLEND);

    float fw = 0.0f, fh = 0.0f;
    SDL_GetTextureSize(g->player_tex, &fw, &fh);

    g->player_tex_w = (int)(fw + 0.5f);
    g->player_tex_h = (int)(fh + 0.5f);

    // Your player.png is a classic 3x4 sheet of 32x32 frames (96x128).
    // If you ever swap art, these 2 lines are what you’ll tweak.
    g->player_frame_w = 32;
    g->player_frame_h = 32;

    // Draw size in world pixels: match tile size (set later once we know map tile size).
    g->player_draw_w = 0.0f;
    g->player_draw_h = 0.0f;

    g->player_tex_ok = true;

    SDL_Log("Player sprite loaded: assets/sprites/player.png (%dx%d) frame=%dx%d",
            g->player_tex_w, g->player_tex_h, g->player_frame_w, g->player_frame_h);

    return true;
}

bool Game_Init(Game* g)
{
    if (!g) return false;

    if (g->player_speed <= 0.0f)
        g->player_speed = 220.0f;

    // Allocate map if not provided.
    if (!g->map)
    {
        g->map = (LayeredMap*)SDL_calloc(1, sizeof(LayeredMap));
        if (!g->map)
        {
            SDL_Log("Game_Init failed: out of memory allocating LayeredMap");
            return false;
        }
    }

    // Load map file.
    if (!LayeredMap_LoadFromFile(g->map, "assets/maps/test.map3"))
    {
        SDL_Log("Game_Init failed: LayeredMap_LoadFromFile assets/maps/test.map3");
        return false;
    }

    SDL_Log("Loaded map assets/maps/test.map3: %dx%d ts=%d sections: ground=1 deco=1 coll=1 interact=1",
            g->map->width, g->map->height, g->map->tile_size);

    // Default player start.
    if (g->player_x == 0.0f && g->player_y == 0.0f)
    {
        g->player_x = 4.0f * (float)g->map->tile_size;
        g->player_y = 4.0f * (float)g->map->tile_size;
    }

    // Default facing.
    if (g->facing < FACE_DOWN || g->facing > FACE_UP)
        g->facing = FACE_DOWN;

    Interaction_Init(&s_interact);

    SDL_Log("Game_Init OK");
    return true;
}

void Game_Shutdown(Game* g)
{
    if (!g) return;

    unload_tileset();

    if (g->player_tex)
    {
        SDL_DestroyTexture(g->player_tex);
        g->player_tex = NULL;
    }
    g->player_tex_ok = false;

    if (g->map)
    {
        LayeredMap_Shutdown(g->map);
        SDL_free(g->map);
        g->map = NULL;
    }
}

void Game_FixedUpdate(Game* g, PlatformApp* app, double dt)
{
    if (!g || !app || !g->map) return;

    if (Input_Pressed(&app->input, SDL_SCANCODE_F1))
        g->debug_collision = !g->debug_collision;

    // Interaction runs first (so E/Esc works even if you’re not moving).
    Interaction_Update(&s_interact, app, g->map, g->player_x, g->player_y);

    // Freeze movement while dialog is open.
    if (!Interaction_IsDialogOpen(&s_interact))
    {
        bool moving = false;
        //move_player(g, app, dt, &moving);
        move_player(g, app, dt);

        // Simple walk animation clock: you can refine later.
        static float s_walk_t = 0.0f;
        if (moving) s_walk_t += (float)dt;
        else s_walk_t = 0.0f;
    }
}

void Game_Render(Game* g, PlatformApp* app)
{
    if (!g || !app || !g->map) return;

    SDL_Renderer* r = app->renderer;
    const LayeredMap* m = g->map;
    const int ts = m->tile_size;

    // Lazy-load tiles + player sprite once renderer exists.
    if (!g_tiles_tex) (void)load_tileset(r, "assets/tiles/tileset.png", ts);
    (void)load_player_sprite(g, r);

    // If we didn’t have draw size yet, match it to map tile size.
    if (g->player_draw_w <= 0.0f) g->player_draw_w = (float)ts;
    if (g->player_draw_h <= 0.0f) g->player_draw_h = (float)ts;

    float cam_x = 0.0f, cam_y = 0.0f;
    camera_calc(m, g->player_x, g->player_y, app->win_w, app->win_h, &cam_x, &cam_y);

    // Visible tile bounds.
    const int tx0 = (int)floorf(cam_x / (float)ts);
    const int ty0 = (int)floorf(cam_y / (float)ts);
    const int tx1 = (int)ceilf((cam_x + (float)app->win_w) / (float)ts) + 1;
    const int ty1 = (int)ceilf((cam_y + (float)app->win_h) / (float)ts) + 1;

    // ---- Pass 1: ground
    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int gid = LayeredMap_Ground(m, tx, ty);
            const float dx = (float)(tx * ts) - cam_x;
            const float dy = (float)(ty * ts) - cam_y;
            draw_tile_1based(r, gid, ts, dx, dy);
        }
    }

    // ---- Pass 2: deco tiles that are "behind" the player by Y (simple depth)
    // Rule: if the tile’s bottom edge is <= player feet Y, draw it before player.
    const float player_feet_y = g->player_y + g->player_draw_h * 0.85f;

    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int did = LayeredMap_Deco(m, tx, ty);
            if (did <= 0) continue;

            const float tile_bottom_y = (float)((ty + 1) * ts);
            if (tile_bottom_y > player_feet_y) continue;

            const float dx = (float)(tx * ts) - cam_x;
            const float dy = (float)(ty * ts) - cam_y;
            draw_tile_1based(r, did, ts, dx, dy);
        }
    }

    // ---- Player sprite
    if (g->player_tex)
    {
        // 3 columns (idle/walk1/walk2), 4 rows (down/left/right/up)
        // Walking animation based on a small timer derived from position.
        // (Cheap, deterministic, and good enough for now.)
        int col = 0;
        {
            const float wobble = fabsf(sinf((g->player_x + g->player_y) * 0.02f));
            col = (wobble > 0.66f) ? 2 : (wobble > 0.33f) ? 1 : 0;
        }

        int row = 0;
        switch (g->facing)
        {
            case FACE_DOWN:  row = 0; break;
            case FACE_LEFT:  row = 1; break;
            case FACE_RIGHT: row = 2; break;
            case FACE_UP:    row = 3; break;
            default:         row = 0; break;
        }

        SDL_FRect src = {
            (float)(col * g->player_frame_w),
            (float)(row * g->player_frame_h),
            (float)g->player_frame_w,
            (float)g->player_frame_h
        };

        // World -> screen (camera)
        const float px = g->player_x - cam_x;
        const float py = g->player_y - cam_y;

        SDL_FRect dst = {
            px,
            py,
            g->player_draw_w,
            g->player_draw_h
        };

        SDL_RenderTexture(r, g->player_tex, &src, &dst);
    }

    // ---- Pass 3: deco tiles that are "in front" of the player
    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int did = LayeredMap_Deco(m, tx, ty);
            if (did <= 0) continue;

            const float tile_bottom_y = (float)((ty + 1) * ts);
            if (tile_bottom_y <= player_feet_y) continue;

            const float dx = (float)(tx * ts) - cam_x;
            const float dy = (float)(ty * ts) - cam_y;
            draw_tile_1based(r, did, ts, dx, dy);
        }
    }

    // Optional collision debug overlay
    if (g->debug_collision)
    {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 255, 0, 0, 90);

        for (int ty = ty0; ty < ty1; ++ty)
        {
            for (int tx = tx0; tx < tx1; ++tx)
            {
                if (!LayeredMap_Solid(m, tx, ty)) continue;

                const float dx = (float)(tx * ts) - cam_x;
                const float dy = (float)(ty * ts) - cam_y;

                SDL_FRect rc = { dx, dy, (float)ts, (float)ts };
                SDL_RenderFillRect(r, &rc);
            }
        }

    if (g->debug_collision)
    {
        const int ts = m->tile_size;
        SDL_FRect feet = Collision_PlayerFeetHitbox(g->player_x, g->player_y, ts);
        feet.x -= cam_x;
        feet.y -= cam_y;

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 255, 0, 140);
        SDL_RenderRect(r, &feet);
    }

    }

    // UI overlay (prompt + message box)
    Interaction_RenderHUD(&s_interact, r, app->win_w, app->win_h);
}
