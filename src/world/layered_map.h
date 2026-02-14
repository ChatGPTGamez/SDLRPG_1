// src/world/layered_map.h
#pragma once
#include <stdbool.h>

typedef struct LayeredMap
{
    int width;
    int height;
    int tile_size;

    int* ground;     // width*height
    int* deco;       // width*height
    int* coll;       // width*height (0/1)
    int* interact;   // width*height (0=none, 1=sign, 2=npc, 3=chest, ...)
} LayeredMap;

bool LayeredMap_Init(LayeredMap* m, int width, int height, int tile_size);
void LayeredMap_Shutdown(LayeredMap* m);

bool LayeredMap_LoadFromFile(LayeredMap* m, const char* path);

// Safe accessors (return 0 if out-of-bounds)
int  LayeredMap_Ground(const LayeredMap* m, int tx, int ty);
int  LayeredMap_Deco(const LayeredMap* m, int tx, int ty);
int  LayeredMap_Interact(const LayeredMap* m, int tx, int ty);
bool LayeredMap_Solid(const LayeredMap* m, int tx, int ty);

// World-space query (pixels)
bool LayeredMap_SolidAtWorld(const LayeredMap* m, float wx, float wy);
int  LayeredMap_InteractAtWorld(const LayeredMap* m, float wx, float wy);
