#pragma once
#include <stdbool.h>

typedef struct Tilemap
{
    int width;
    int height;
    int tile_size;
    int* tiles; // width*height
} Tilemap;

bool Tilemap_Init(Tilemap* m, int w, int h, int tile_size);
void Tilemap_Shutdown(Tilemap* m);

int  Tilemap_Get(const Tilemap* m, int tx, int ty);   // out of bounds => solid wall (1)
void Tilemap_Set(Tilemap* m, int tx, int ty, int id);

bool Tilemap_IsSolidId(int id);
bool Tilemap_IsSolidTile(const Tilemap* m, int tx, int ty);
bool Tilemap_IsSolidAtWorld(const Tilemap* m, float wx, float wy);

// NEW: load from text file
bool Tilemap_LoadFromFile(Tilemap* m, const char* path);
