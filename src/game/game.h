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
    // Map
    LayeredMap* map;

    // Player (kept for compatibility with existing interaction / camera / UI)
    float player_x;
    float player_y;
    float player_speed;
    PlayerFacing facing;

    // Debug
    bool debug_collision; // F1 toggle

    // Interaction (your existing system)
    InteractionSystem interact;

    // Entities
    EntitySystem ents;
    int player_eid;

} Game;

bool Game_Init(Game* g);
void Game_Shutdown(Game* g);

void Game_FixedUpdate(Game* g, PlatformApp* app, double dt);
void Game_Render(Game* g, PlatformApp* app);
