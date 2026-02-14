// src/game/game.h
#pragma once
#include <stdbool.h>

typedef struct PlatformApp PlatformApp;
typedef struct Camera2D Camera2D;
typedef struct LayeredMap LayeredMap;
typedef struct Tileset Tileset;
typedef struct SDL_Texture SDL_Texture;

#include "../ui/message_box.h"

typedef enum PlayerFacing
{
    FACE_DOWN = 0,
    FACE_LEFT = 1,
    FACE_RIGHT = 2,
    FACE_UP = 3
} PlayerFacing;

typedef struct Game
{
    float player_x;
    float player_y;
    float player_speed;

    Camera2D* cam;
    LayeredMap* map;

    Tileset* tileset;
    bool tileset_ok;

    SDL_Texture* player_tex;
    bool player_tex_ok;
    int player_tex_w;
    int player_tex_h;

    int player_frame_w;
    int player_frame_h;

    float player_draw_w;
    float player_draw_h;

    PlayerFacing facing;

    bool debug_collision;

    MessageBox msg;   // NEW: interaction feedback overlay (text later)
} Game;

bool Game_Init(Game* g);
void Game_FixedUpdate(Game* g, PlatformApp* app, double dt);
void Game_Render(Game* g, PlatformApp* app);
void Game_Shutdown(Game* g);
