// src/world/tilemap.c
#include "tilemap.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static int idx(const Tilemap* m, int tx, int ty)
{
    return ty * m->width + tx;
}

bool Tilemap_Init(Tilemap* m, int w, int h, int tile_size)
{
    if (!m || w <= 0 || h <= 0 || tile_size <= 0) return false;

    m->width = w;
    m->height = h;
    m->tile_size = tile_size;
    m->tiles = (int*)SDL_malloc((size_t)(w * h) * sizeof(int));
    if (!m->tiles) return false;

    for (int i = 0; i < w * h; ++i) m->tiles[i] = 0;
    return true;
}

void Tilemap_Shutdown(Tilemap* m)
{
    if (!m) return;
    if (m->tiles) SDL_free(m->tiles);
    m->tiles = NULL;
    m->width = m->height = m->tile_size = 0;
}

int Tilemap_Get(const Tilemap* m, int tx, int ty)
{
    if (!m || !m->tiles) return 1;
    if (tx < 0 || ty < 0 || tx >= m->width || ty >= m->height) return 1;
    return m->tiles[idx(m, tx, ty)];
}

void Tilemap_Set(Tilemap* m, int tx, int ty, int id)
{
    if (!m || !m->tiles) return;
    if (tx < 0 || ty < 0 || tx >= m->width || ty >= m->height) return;
    m->tiles[idx(m, tx, ty)] = id;
}

bool Tilemap_IsSolidId(int id)
{
    return (id == 1) || (id == 3);
}

bool Tilemap_IsSolidTile(const Tilemap* m, int tx, int ty)
{
    return Tilemap_IsSolidId(Tilemap_Get(m, tx, ty));
}

bool Tilemap_IsSolidAtWorld(const Tilemap* m, float wx, float wy)
{
    if (!m) return true;
    const int ts = m->tile_size;
    const int tx = (int)(wx / (float)ts);
    const int ty = (int)(wy / (float)ts);
    return Tilemap_IsSolidTile(m, tx, ty);
}

bool Tilemap_LoadFromFile(Tilemap* m, const char* path)
{
    if (!m || !path) return false;

    FILE* f = fopen(path, "r");
    if (!f) return false;

    int w = 0, h = 0, ts = 0;
    if (fscanf(f, "%d %d %d", &w, &h, &ts) != 3 || w <= 0 || h <= 0 || ts <= 0)
    {
        fclose(f);
        return false;
    }

    // Re-init map storage
    Tilemap_Shutdown(m);
    if (!Tilemap_Init(m, w, h, ts))
    {
        fclose(f);
        return false;
    }

    // Read tiles row-major
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int id = 0;
            if (fscanf(f, "%d", &id) != 1)
            {
                fclose(f);
                return false;
            }
            Tilemap_Set(m, x, y, id);
        }
    }

    fclose(f);
    return true;
}
