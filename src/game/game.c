// src/game/game.c
#include "game.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <string.h>

#include "../platform/platform_app.h"
#include "../platform/platform_input.h"
#include "../render/camera2d.h"
#include "../render/tileset.h"
#include "../world/layered_map.h"
#include "interaction.h"
#include "../ui/message_box.h"

static const float PLAYER_W = 24.0f;
static const float PLAYER_H = 24.0f;

// -------- collision ----------
static bool aabb_hits_solid(const LayeredMap* m, float x, float y, float w, float h)
{
    if (LayeredMap_SolidAtWorld(m, x,     y))     return true;
    if (LayeredMap_SolidAtWorld(m, x+w,   y))     return true;
    if (LayeredMap_SolidAtWorld(m, x,     y+h))   return true;
    if (LayeredMap_SolidAtWorld(m, x+w,   y+h))   return true;
    return false;
}

static void move_with_collision(const LayeredMap* m, float* px, float* py, float dx, float dy)
{
    float x = *px;
    float y = *py;

    if (dx != 0.0f)
    {
        float nx = x + dx;
        if (!aabb_hits_solid(m, nx, y, PLAYER_W, PLAYER_H)) x = nx;
    }

    if (dy != 0.0f)
    {
        float ny = y + dy;
        if (!aabb_hits_solid(m, x, ny, PLAYER_W, PLAYER_H)) y = ny;
    }

    *px = x;
    *py = y;
}

// -------- spawn ----------
static void spawn_player_safely(Game* g)
{
    const LayeredMap* m = g->map;
    const int ts = m->tile_size;

    for (int ty = 1; ty < m->height - 1; ++ty)
    {
        for (int tx = 1; tx < m->width - 1; ++tx)
        {
            if (LayeredMap_Solid(m, tx, ty)) continue;

            const float px = (float)(tx * ts) + (ts - PLAYER_W) * 0.5f;
            const float py = (float)(ty * ts) + (ts - PLAYER_H) * 0.5f;

            if (!aabb_hits_solid(m, px, py, PLAYER_W, PLAYER_H))
            {
                g->player_x = px;
                g->player_y = py;
                return;
            }
        }
    }

    g->player_x = 64.0f;
    g->player_y = 64.0f;
}

// -------- camera clamp ----------
static void clamp_camera_to_world(Game* g)
{
    const float world_w = (float)(g->map->width  * g->map->tile_size);
    const float world_h = (float)(g->map->height * g->map->tile_size);

    if ((float)g->cam->viewport_w >= world_w)
        g->cam->x = (world_w - (float)g->cam->viewport_w) * 0.5f;
    else
    {
        if (g->cam->x < 0.0f) g->cam->x = 0.0f;
        const float max_x = world_w - (float)g->cam->viewport_w;
        if (g->cam->x > max_x) g->cam->x = max_x;
    }

    if ((float)g->cam->viewport_h >= world_h)
        g->cam->y = (world_h - (float)g->cam->viewport_h) * 0.5f;
    else
    {
        if (g->cam->y < 0.0f) g->cam->y = 0.0f;
        const float max_y = world_h - (float)g->cam->viewport_h;
        if (g->cam->y > max_y) g->cam->y = max_y;
    }
}

// -------- player sprite ----------
static void player_load_sprite(Game* g, SDL_Renderer* r)
{
    if (g->player_tex_ok) return;

    g->player_tex = IMG_LoadTexture(r, "assets/sprites/player.png");
    if (!g->player_tex)
    {
        SDL_Log("Player sprite missing: %s", SDL_GetError());
        return;
    }

    SDL_SetTextureBlendMode(g->player_tex, SDL_BLENDMODE_BLEND);

    float fw = 0.0f, fh = 0.0f;
    if (!SDL_GetTextureSize(g->player_tex, &fw, &fh))
    {
        SDL_Log("SDL_GetTextureSize(player) failed: %s", SDL_GetError());
        SDL_DestroyTexture(g->player_tex);
        g->player_tex = NULL;
        return;
    }

    g->player_tex_w = (int)fw;
    g->player_tex_h = (int)fh;

    g->player_frame_w = 32;
    g->player_frame_h = 32;
    g->player_draw_w = 32.0f;
    g->player_draw_h = 32.0f;

    g->player_tex_ok = true;
}

static void player_render(Game* g, PlatformApp* app)
{
    float sx, sy;
    Camera2D_WorldToScreen(g->cam, g->player_x, g->player_y, &sx, &sy);

    const float aabb_cx = sx + PLAYER_W * 0.5f;
    const float aabb_bottom = sy + PLAYER_H;

    SDL_FRect dst = {
        aabb_cx - g->player_draw_w * 0.5f,
        aabb_bottom - g->player_draw_h,
        g->player_draw_w,
        g->player_draw_h
    };

    if (g->player_tex_ok && g->player_tex)
    {
        const int cols = g->player_tex_w / g->player_frame_w;
        const int rows = g->player_tex_h / g->player_frame_h;

        if (cols >= 1 && rows >= 4)
        {
            SDL_FRect src = {
                0.0f,
                (float)((int)g->facing * g->player_frame_h),
                (float)g->player_frame_w,
                (float)g->player_frame_h
            };
            SDL_RenderTexture(app->renderer, g->player_tex, &src, &dst);
        }
        else
        {
            SDL_RenderTexture(app->renderer, g->player_tex, NULL, &dst);
        }
    }
    else
    {
        SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
        SDL_FRect box = { sx, sy, PLAYER_W, PLAYER_H };
        SDL_RenderFillRect(app->renderer, &box);
    }
}

// -------- rendering: minimal for now --------
// We assume you already have a tileset + map draw working from earlier.
// If your current project had y-sort + tall objects, we’ll re-add those as separate render module next.
// For now: draw ground+deco simply, then player, then UI. (Keeps this file short.)

static void draw_map_simple(Game* g, PlatformApp* app)
{
    const LayeredMap* m = g->map;
    const int ts = m->tile_size;

    const bool use_tiles = (g->tileset_ok && g->tileset && g->tileset->ok);

    // Visible bounds (simple)
    const float wx0 = g->cam->x;
    const float wy0 = g->cam->y;
    const float wx1 = g->cam->x + (float)g->cam->viewport_w;
    const float wy1 = g->cam->y + (float)g->cam->viewport_h;

    int tx0 = (int)(wx0 / ts) - 1;
    int ty0 = (int)(wy0 / ts) - 1;
    int tx1 = (int)(wx1 / ts) + 1;
    int ty1 = (int)(wy1 / ts) + 1;

    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 > m->width - 1)  tx1 = m->width - 1;
    if (ty1 > m->height - 1) ty1 = m->height - 1;

    // Ground
    for (int ty = ty0; ty <= ty1; ++ty)
    {
        for (int tx = tx0; tx <= tx1; ++tx)
        {
            const int id = LayeredMap_Ground(m, tx, ty);

            float sx, sy;
            Camera2D_WorldToScreen(g->cam, (float)(tx * ts), (float)(ty * ts), &sx, &sy);

            SDL_FRect dst = { sx, sy, (float)ts, (float)ts };

            if (use_tiles) Tileset_DrawTile(g->tileset, app->renderer, id, &dst);
            else { SDL_SetRenderDrawColor(app->renderer, 35, 35, 35, 255); SDL_RenderFillRect(app->renderer, &dst); }
        }
    }

    // Deco
    for (int ty = ty0; ty <= ty1; ++ty)
    {
        for (int tx = tx0; tx <= tx1; ++tx)
        {
            const int id = LayeredMap_Deco(m, tx, ty);
            if (id == 0) continue;

            float sx, sy;
            Camera2D_WorldToScreen(g->cam, (float)(tx * ts), (float)(ty * ts), &sx, &sy);

            SDL_FRect dst = { sx, sy, (float)ts, (float)ts };

            if (use_tiles) Tileset_DrawTile(g->tileset, app->renderer, id, &dst);
            else { SDL_SetRenderDrawColor(app->renderer, 90, 90, 90, 255); SDL_RenderFillRect(app->renderer, &dst); }
        }
    }
}

static void draw_interact_prompt(Game* g, PlatformApp* app)
{
    // If message box is open, don’t show prompt
    if (g->msg.visible) return;

    // Check if something is interactable in front
    const InteractProbe p = Interaction_ProbeFront(g);
    if (!p.valid || p.id == 0) return;

    // Simple prompt indicator: a small yellow bar above player (until SDL_ttf)
    float sx, sy;
    Camera2D_WorldToScreen(g->cam, g->player_x, g->player_y, &sx, &sy);

    SDL_FRect bar = { sx + 2.0f, sy - 10.0f, PLAYER_W + 8.0f, 6.0f };
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(app->renderer, 255, 220, 80, 200);
    SDL_RenderFillRect(app->renderer, &bar);
}

bool Game_Init(Game* g)
{
    if (!g) return false;
    memset(g, 0, sizeof(*g));

    g->player_speed = 220.0f;
    g->facing = FACE_DOWN;
    g->debug_collision = false;

    MessageBox_Init(&g->msg);

    static Camera2D s_cam;
    g->cam = &s_cam;
    g->cam->x = 0.0f;
    g->cam->y = 0.0f;
    g->cam->viewport_w = 1280;
    g->cam->viewport_h = 720;

    static LayeredMap s_map;
    g->map = &s_map;

    if (!LayeredMap_LoadFromFile(g->map, "assets/maps/test.map3"))
    {
        SDL_Log("FAILED to load assets/maps/test.map3");
        return false;
    }

    spawn_player_safely(g);

    static Tileset s_tileset;
    g->tileset = &s_tileset;
    g->tileset_ok = false;

    g->player_tex = NULL;
    g->player_tex_ok = false;

    SDL_Log("Game_Init OK");
    return true;
}

void Game_FixedUpdate(Game* g, PlatformApp* app, double dt)
{
    if (!app->has_focus) return;

    const PlatformInput* in = &app->input;

    // Toggle message box with E if open; else try interact
    if (Input_Pressed(in, SDL_SCANCODE_E))
    {
        (void)Interaction_Try(g);
    }

    // If message is open, freeze movement (classic RPG behavior)
    if (g->msg.visible)
    {
        if (Input_Pressed(in, SDL_SCANCODE_ESCAPE))
            MessageBox_Hide(&g->msg);
        return;
    }

    float ax = 0.0f, ay = 0.0f;
    const bool up    = Input_Down(in, SDL_SCANCODE_W) || Input_Down(in, SDL_SCANCODE_UP);
    const bool down  = Input_Down(in, SDL_SCANCODE_S) || Input_Down(in, SDL_SCANCODE_DOWN);
    const bool left  = Input_Down(in, SDL_SCANCODE_A) || Input_Down(in, SDL_SCANCODE_LEFT);
    const bool right = Input_Down(in, SDL_SCANCODE_D) || Input_Down(in, SDL_SCANCODE_RIGHT);

    if (up)    ay -= 1.0f;
    if (down)  ay += 1.0f;
    if (left)  ax -= 1.0f;
    if (right) ax += 1.0f;

    if (up && !down) g->facing = FACE_UP;
    else if (down && !up) g->facing = FACE_DOWN;
    else if (left && !right) g->facing = FACE_LEFT;
    else if (right && !left) g->facing = FACE_RIGHT;

    if (ax != 0.0f && ay != 0.0f) { ax *= 0.70710678f; ay *= 0.70710678f; }

    const float dx = ax * g->player_speed * (float)dt;
    const float dy = ay * g->player_speed * (float)dt;

    move_with_collision(g->map, &g->player_x, &g->player_y, dx, dy);

    if (Input_Pressed(in, SDL_SCANCODE_ESCAPE))
        app->running = false;
}

void Game_Render(Game* g, PlatformApp* app)
{
    // Lazy-load tileset
    if (g->tileset && !g->tileset_ok)
    {
        g->tileset_ok = Tileset_Load(g->tileset, app->renderer, "assets/tiles/tileset.png",
                                     g->map->tile_size, g->map->tile_size);
        if (!g->tileset_ok)
            SDL_Log("Tileset not loaded; using fallback.");
    }

    player_load_sprite(g, app->renderer);

    int ww = 0, wh = 0;
    SDL_GetWindowSize(app->window, &ww, &wh);
    Camera2D_SetViewport(g->cam, ww, wh);

    // Follow player
    const float pcx = g->player_x + PLAYER_W * 0.5f;
    const float pcy = g->player_y + PLAYER_H * 0.5f;
    Camera2D_CenterOn(g->cam, pcx, pcy);
    clamp_camera_to_world(g);

    draw_map_simple(g, app);
    player_render(g, app);
    draw_interact_prompt(g, app);

    // UI last
    MessageBox_Render(&g->msg, app->renderer, ww, wh);
}

void Game_Shutdown(Game* g)
{
    if (!g) return;

    if (g->player_tex)
    {
        SDL_DestroyTexture(g->player_tex);
        g->player_tex = NULL;
    }
    g->player_tex_ok = false;

    if (g->tileset)
        Tileset_Unload(g->tileset);

    if (g->map)
        LayeredMap_Shutdown(g->map);
}
