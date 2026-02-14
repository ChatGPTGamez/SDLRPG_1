// src/game/game.c
#include "game/game.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <string.h>
#include <math.h>

#include "world/layered_map.h"
#include "platform/platform_app.h"
#include "game/interaction.h"

static InteractionSystem s_interact;

static SDL_Texture* g_tiles_tex = NULL;
static int g_tiles_cols = 0;
static int g_tiles_rows = 0;
static bool g_debug_coll = false;

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
    g_tiles_cols = g_tiles_rows = 0;
}

static void draw_tile(SDL_Renderer* r, int tile_id, int tile_size, float dst_x, float dst_y)
{
    if (!g_tiles_tex) return;
    if (tile_id <= 0) return;

    const int idx = tile_id - 1;
    if (g_tiles_cols <= 0) return;

    const int sx = (idx % g_tiles_cols) * tile_size;
    const int sy = (idx / g_tiles_cols) * tile_size;

    SDL_FRect src = { (float)sx, (float)sy, (float)tile_size, (float)tile_size };
    SDL_FRect dst = { dst_x, dst_y, (float)tile_size, (float)tile_size };

    SDL_RenderTexture(r, g_tiles_tex, &src, &dst);
}

static void camera_calc(const LayeredMap* m, float px, float py, int screen_w, int screen_h, float* out_cam_x, float* out_cam_y)
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

static void move_player(Game* g, const PlatformApp* app, double dt)
{
    const PlatformInput* in = &app->input;

    float dx = 0.0f, dy = 0.0f;
    if (Input_Down(in, SDL_SCANCODE_A) || Input_Down(in, SDL_SCANCODE_LEFT))  dx -= 1.0f;
    if (Input_Down(in, SDL_SCANCODE_D) || Input_Down(in, SDL_SCANCODE_RIGHT)) dx += 1.0f;
    if (Input_Down(in, SDL_SCANCODE_W) || Input_Down(in, SDL_SCANCODE_UP))    dy -= 1.0f;
    if (Input_Down(in, SDL_SCANCODE_S) || Input_Down(in, SDL_SCANCODE_DOWN))  dy += 1.0f;

    const float len = SDL_sqrtf(dx*dx + dy*dy);
    if (len > 0.0001f) { dx /= len; dy /= len; }

    const float step = g->player_speed * (float)dt;

    g->player_x += dx * step;
    g->player_y += dy * step;

    const LayeredMap* m = g->map;
    const float max_x = (float)(m->width * m->tile_size - m->tile_size);
    const float max_y = (float)(m->height * m->tile_size - m->tile_size);

    if (g->player_x < 0) g->player_x = 0;
    if (g->player_y < 0) g->player_y = 0;
    if (g->player_x > max_x) g->player_x = max_x;
    if (g->player_y > max_y) g->player_y = max_y;
}

bool Game_Init(Game* g)
{
    if (!g) return false;

    // defaults
    if (g->player_speed <= 0.0f) g->player_speed = 220.0f;

    // Allocate map struct if not present
    if (!g->map)
    {
        g->map = (LayeredMap*)SDL_calloc(1, sizeof(LayeredMap));
        if (!g->map)
        {
            SDL_Log("Game_Init failed: out of memory allocating LayeredMap");
            return false;
        }
    }

    // Load map contents
    if (!LayeredMap_LoadFromFile(g->map, "assets/maps/test.map3"))
    {
        SDL_Log("Game_Init failed: LayeredMap_LoadFromFile assets/maps/test.map3");
        return false;
    }

    SDL_Log("Loaded map assets/maps/test.map3: %dx%d ts=%d sections: ground=1 deco=1 coll=1 interact=1",
            g->map->width, g->map->height, g->map->tile_size);

    // Player start if unset
    if (g->player_x == 0.0f && g->player_y == 0.0f)
    {
        g->player_x = 4.0f * (float)g->map->tile_size;
        g->player_y = 4.0f * (float)g->map->tile_size;
    }

    Interaction_Init(&s_interact);

    SDL_Log("Game_Init OK");
    return true;
}

void Game_Shutdown(Game* g)
{
    if (!g) return;

    unload_tileset();

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
        g_debug_coll = !g_debug_coll;

    Interaction_Update(&s_interact, app, g->map, g->player_x, g->player_y);

    if (!Interaction_IsDialogOpen(&s_interact))
        move_player(g, app, dt);
}

void Game_Render(Game* g, PlatformApp* app)
{
    if (!g || !app || !g->map) return;

    SDL_Renderer* r = app->renderer;
    const LayeredMap* m = g->map;

    if (!g_tiles_tex)
        (void)load_tileset(r, "assets/tiles/tileset.png", m->tile_size);

    float cam_x = 0.0f, cam_y = 0.0f;
    camera_calc(m, g->player_x, g->player_y, app->win_w, app->win_h, &cam_x, &cam_y);

    const int ts = m->tile_size;
    const int tx0 = (int)floorf(cam_x / (float)ts);
    const int ty0 = (int)floorf(cam_y / (float)ts);
    const int tx1 = (int)ceilf((cam_x + (float)app->win_w) / (float)ts) + 1;
    const int ty1 = (int)ceilf((cam_y + (float)app->win_h) / (float)ts) + 1;

    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int gid = LayeredMap_Ground(m, tx, ty);
            const float dx = (float)(tx * ts) - cam_x;
            const float dy = (float)(ty * ts) - cam_y;
            draw_tile(r, gid, ts, dx, dy);
        }
    }

    for (int ty = ty0; ty < ty1; ++ty)
    {
        for (int tx = tx0; tx < tx1; ++tx)
        {
            const int did = LayeredMap_Deco(m, tx, ty);
            const float dx = (float)(tx * ts) - cam_x;
            const float dy = (float)(ty * ts) - cam_y;
            draw_tile(r, did, ts, dx, dy);

            if (g_debug_coll && LayeredMap_Solid(m, tx, ty))
            {
                SDL_FRect rc = { dx, dy, (float)ts, (float)ts };
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 255, 0, 0, 90);
                SDL_RenderFillRect(r, &rc);
            }
        }
    }

    SDL_FRect pr = {
        g->player_x - cam_x + (float)ts * 0.25f,
        g->player_y - cam_y + (float)ts * 0.20f,
        (float)ts * 0.50f,
        (float)ts * 0.60f
    };
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &pr);

    Interaction_RenderHUD(&s_interact, r, app->win_w, app->win_h);
}
