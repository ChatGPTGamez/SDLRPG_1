#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef enum EntityType
{
    ENT_NONE = 0,
    ENT_PLAYER,
    ENT_NPC,
    ENT_DOOR,
    ENT_CHEST
} EntityType;

enum
{
    ENT_FLAG_SOLID        = 1 << 0, // blocks movement
    ENT_FLAG_INTERACTABLE = 1 << 1  // can press E near it
};

typedef struct Entity
{
    int        id;
    EntityType type;
    unsigned   flags;
    bool       alive;

    // World position of the entity "origin" (same convention as your player_x/player_y)
    float x, y;

    // Visual footprint (for debug / placeholder rendering)
    float w, h;

    // Feet hitbox tuning (relative to tile size)
    // defaults mimic the collision feet box you already used:
    // x+0.25*ts, y+0.55*ts, w=0.50*ts, h=0.35*ts
    float feet_off_x;
    float feet_off_y;
    float feet_w;
    float feet_h;

    // Optional label (NPC name, door name, etc.)
    char name[32];
} Entity;

SDL_FRect Entity_FeetHitbox(const Entity* e, int tile_size);
SDL_FRect Entity_VisualRect(const Entity* e);
