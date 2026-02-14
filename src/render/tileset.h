#pragma once
#include <stdbool.h>

typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_FRect SDL_FRect;

typedef struct Tileset
{
    SDL_Texture* tex;
    int tile_w;
    int tile_h;
    int tex_w;
    int tex_h;
    int cols;
    int rows;
    bool ok;
} Tileset;

bool Tileset_Load(Tileset* t, SDL_Renderer* r, const char* png_path, int tile_w, int tile_h);
void Tileset_Unload(Tileset* t);

// Draw tile by index at destination rect (screen space)
void Tileset_DrawTile(const Tileset* t, SDL_Renderer* r, int tile_id, const SDL_FRect* dst);
