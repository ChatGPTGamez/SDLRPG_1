#pragma once
#include "game/entity.h"

#ifndef ENTITY_MAX
#define ENTITY_MAX 256
#endif

typedef struct EntitySystem
{
    Entity entities[ENTITY_MAX];
    int    count;
    int    next_id;
} EntitySystem;

void EntitySystem_Init(EntitySystem* es);

Entity* EntitySystem_Spawn(EntitySystem* es, EntityType type, float x, float y);
Entity* EntitySystem_FindById(EntitySystem* es, int id);

// Returns number of ids written.
int  EntitySystem_BuildRenderListY(EntitySystem* es, int* out_ids, int max_ids);

// Basic solid collision: push mover out of other solids using feet hitboxes.
void EntitySystem_ResolveSolids(EntitySystem* es, const Entity* mover, int tile_size);

// Interact query: finds nearest interactable within radius (world units).
Entity* EntitySystem_FindNearestInteractable(EntitySystem* es,
                                            const Entity* from,
                                            int tile_size,
                                            float radius_world);
