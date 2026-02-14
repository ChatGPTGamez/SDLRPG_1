// src/render/tileset.c
#include "tileset.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

static void Tileset_Reset(Tileset* t)
{
    t->tex = NULL;
    t->tile_w = 0;
    t->tile_h = 0;
    t->tex_w = 0;
    t->tex_h = 0;
    t->cols = 0;
    t->rows = 0;
    t->ok = false;
}

bool Tileset_Load(Tileset* t, SDL_Renderer* r, const char* png_path, int tile_w, int tile_h)
{
    if (!t || !r || !png_path || tile_w <= 0 || tile_h <= 0)
        return false;

    Tileset_Reset(t);

    // SDL3_image no longer requires IMG_Init
    SDL_Texture* tex = IMG_LoadTexture(r, png_path);
    if (!tex)
    {
        SDL_Log("Tileset_Load failed for '%s': %s", png_path, SDL_GetError());
        return false;
    }

    float fw = 0.0f;
    float fh = 0.0f;

    if (!SDL_GetTextureSize(tex, &fw, &fh))
    {
        SDL_Log("SDL_GetTextureSize failed: %s", SDL_GetError());
        SDL_DestroyTexture(tex);
        return false;
    }

    int w = (int)fw;
    int h = (int)fh;

    if (w < tile_w || h < tile_h)
    {
        SDL_Log("Tileset texture too small (%dx%d vs tile %dx%d)", w, h, tile_w, tile_h);
        SDL_DestroyTexture(tex);
        return false;
    }

    t->tex = tex;
    t->tile_w = tile_w;
    t->tile_h = tile_h;
    t->tex_w = w;
    t->tex_h = h;
    t->cols = w / tile_w;
    t->rows = h / tile_h;
    t->ok = (t->cols > 0 && t->rows > 0);

    SDL_SetTextureBlendMode(t->tex, SDL_BLENDMODE_BLEND);

    SDL_Log("Tileset loaded: %s (%dx%d) grid=%dx%d",
            png_path, w, h, t->cols, t->rows);

    return t->ok;
}

void Tileset_Unload(Tileset* t)
{
    if (!t) return;

    if (t->tex)
        SDL_DestroyTexture(t->tex);

    Tileset_Reset(t);
}

void Tileset_DrawTile(const Tileset* t, SDL_Renderer* r, int tile_id, const SDL_FRect* dst)
{
    if (!t || !t->ok || !t->tex || !r || !dst)
        return;

    const int max_tiles = t->cols * t->rows;
    if (tile_id < 0 || tile_id >= max_tiles)
        return;

    const int sx = (tile_id % t->cols) * t->tile_w;
    const int sy = (tile_id / t->cols) * t->tile_h;

    SDL_FRect src = {
        (float)sx,
        (float)sy,
        (float)t->tile_w,
        (float)t->tile_h
    };

    SDL_RenderTexture(r, t->tex, &src, dst);
}
