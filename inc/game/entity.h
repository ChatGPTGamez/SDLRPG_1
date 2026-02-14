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
    ENT_FLAG_SOLID        = 1 << 0,
    ENT_FLAG_INTERACTABLE = 1 << 1
};

typedef struct Entity
{
    int        id;
    EntityType type;
    unsigned   flags;
    bool       alive;

    float x, y;   // world origin
    float w, h;   // visual size (placeholder)

    // Feet hitbox ratios (relative to tile size)
    float feet_off_x;
    float feet_off_y;
    float feet_w;
    float feet_h;

    char name[32];

    // ---- Door data (only used when type == ENT_DOOR)
    char  door_target_map[128];
    float door_spawn_x;   // world coords
    float door_spawn_y;

} Entity;

SDL_FRect Entity_FeetHitbox(const Entity* e, int tile_size);
SDL_FRect Entity_VisualRect(const Entity* e);
