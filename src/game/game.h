// src/game/game.h
#pragma once
#include <stdbool.h>

typedef struct PlatformApp PlatformApp;
typedef struct LayeredMap LayeredMap;

#include "game/entity_system.h"
#include "game/interaction.h"

typedef enum PlayerFacing
{
    FACE_DOWN = 0,
    FACE_LEFT,
    FACE_RIGHT,
    FACE_UP
} PlayerFacing;

typedef struct Game
{
    LayeredMap* map;

    // Track current map path (for door toggles)
    char current_map[128];

    // Player (legacy fields kept for camera/interaction/UI)
    float player_x;
    float player_y;
    float player_speed;
    PlayerFacing facing;

    bool debug_collision;

    InteractionSystem interact;

    EntitySystem ents;
    int player_eid;

} Game;

bool Game_Init(Game* g);
void Game_Shutdown(Game* g);

void Game_FixedUpdate(Game* g, PlatformApp* app, double dt);
void Game_Render(Game* g, PlatformApp* app);
