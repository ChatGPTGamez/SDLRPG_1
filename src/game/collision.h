// src/game/collision.h
#pragma once

#include <SDL3/SDL.h>

typedef struct LayeredMap LayeredMap;

// Build the player's *feet* hitbox from the player's world position (top-left-ish)
// using the map tile size. Tuned to feel good for top-down RPG movement.
SDL_FRect Collision_PlayerFeetHitbox(float player_x, float player_y, int tile_size);

// Move a box by (dx,dy) with tile collision:
// - Move X + resolve
// - Move Y + resolve
// This produces "sliding" automatically when one axis is blocked.
void Collision_MoveBox_Tiles(const LayeredMap* m, SDL_FRect* box, float dx, float dy);
