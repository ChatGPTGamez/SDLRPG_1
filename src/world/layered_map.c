// src/world/layered_map.c
#include "layered_map.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_layers(LayeredMap* m)
{
    if (!m) return;
    free(m->ground);   m->ground = NULL;
    free(m->deco);     m->deco = NULL;
    free(m->coll);     m->coll = NULL;
    free(m->interact); m->interact = NULL;
}

bool LayeredMap_Init(LayeredMap* m, int width, int height, int tile_size)
{
    if (!m || width <= 0 || height <= 0 || tile_size <= 0) return false;

    memset(m, 0, sizeof(*m));
    m->width = width;
    m->height = height;
    m->tile_size = tile_size;

    const size_t n = (size_t)width * (size_t)height;

    m->ground   = (int*)calloc(n, sizeof(int));
    m->deco     = (int*)calloc(n, sizeof(int));
    m->coll     = (int*)calloc(n, sizeof(int));
    m->interact = (int*)calloc(n, sizeof(int));

    if (!m->ground || !m->deco || !m->coll || !m->interact)
    {
        free_layers(m);
        return false;
    }

    return true;
}

void LayeredMap_Shutdown(LayeredMap* m)
{
    if (!m) return;
    free_layers(m);
    memset(m, 0, sizeof(*m));
}

static int idx(const LayeredMap* m, int tx, int ty)
{
    return ty * m->width + tx;
}

int LayeredMap_Ground(const LayeredMap* m, int tx, int ty)
{
    if (!m || !m->ground) return 0;
    if (tx < 0 || ty < 0 || tx >= m->width || ty >= m->height) return 0;
    return m->ground[idx(m, tx, ty)];
}

int LayeredMap_Deco(const LayeredMap* m, int tx, int ty)
{
    if (!m || !m->deco) return 0;
    if (tx < 0 || ty < 0 || tx >= m->width || ty >= m->height) return 0;
    return m->deco[idx(m, tx, ty)];
}

int LayeredMap_Interact(const LayeredMap* m, int tx, int ty)
{
    if (!m || !m->interact) return 0;
    if (tx < 0 || ty < 0 || tx >= m->width || ty >= m->height) return 0;
    return m->interact[idx(m, tx, ty)];
}

bool LayeredMap_Solid(const LayeredMap* m, int tx, int ty)
{
    if (!m || !m->coll) return false;
    if (tx < 0 || ty < 0 || tx >= m->width || ty >= m->height) return true; // outside is solid
    return m->coll[idx(m, tx, ty)] != 0;
}

bool LayeredMap_SolidAtWorld(const LayeredMap* m, float wx, float wy)
{
    if (!m || m->tile_size <= 0) return true;
    const int tx = (int)(wx / (float)m->tile_size);
    const int ty = (int)(wy / (float)m->tile_size);
    return LayeredMap_Solid(m, tx, ty);
}

int LayeredMap_InteractAtWorld(const LayeredMap* m, float wx, float wy)
{
    if (!m || m->tile_size <= 0) return 0;
    const int tx = (int)(wx / (float)m->tile_size);
    const int ty = (int)(wy / (float)m->tile_size);
    return LayeredMap_Interact(m, tx, ty);
}

// ---------- File parsing helpers ----------

static char* read_entire_file(const char* path, size_t* out_size)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) { fclose(f); return NULL; }

    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    buf[rd] = '\0';
    if (out_size) *out_size = rd;
    return buf;
}

static void str_to_upper(char* s)
{
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

// returns pointer to next token (modifies buffer by writing '\0')
static char* next_token(char** cursor)
{
    char* s = *cursor;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) { *cursor = s; return NULL; }

    char* start = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (*s) { *s = '\0'; s++; }
    *cursor = s;
    return start;
}

static bool token_is(const char* t, const char* word)
{
    return t && word && strcmp(t, word) == 0;
}

static bool read_ints(char** cursor, int* dst, int count)
{
    for (int i = 0; i < count; ++i)
    {
        char* tok = next_token(cursor);
        if (!tok) return false;

        // allow commas
        for (char* p = tok; *p; ++p) if (*p == ',') *p = ' ';
        dst[i] = atoi(tok);
    }
    return true;
}

bool LayeredMap_LoadFromFile(LayeredMap* m, const char* path)
{
    if (!m || !path) return false;

    size_t sz = 0;
    char* buf = read_entire_file(path, &sz);
    if (!buf)
    {
        SDL_Log("LayeredMap_LoadFromFile: cannot read %s", path);
        return false;
    }

    // Tokenize in-place; we uppercase only header tokens by copying.
    char* cursor = buf;

    // Expect header: MAP3 width height tile_size (but we also tolerate missing MAP3)
    int width = 0, height = 0, tile_size = 0;

    char* t0 = next_token(&cursor);
    if (!t0) { free(buf); return false; }

    char header[32];
    SDL_strlcpy(header, t0, sizeof(header));
    str_to_upper(header);

    if (token_is(header, "MAP3"))
    {
        char* tw = next_token(&cursor);
        char* th = next_token(&cursor);
        char* ts2 = next_token(&cursor);
        if (!tw || !th || !ts2) { free(buf); return false; }
        width = atoi(tw);
        height = atoi(th);
        tile_size = atoi(ts2);
    }
    else
    {
        // If file starts with width height tile_size
        width = atoi(t0);
        char* th = next_token(&cursor);
        char* ts2 = next_token(&cursor);
        if (!th || !ts2) { free(buf); return false; }
        height = atoi(th);
        tile_size = atoi(ts2);
    }

    if (width <= 0 || height <= 0 || tile_size <= 0)
    {
        SDL_Log("LayeredMap_LoadFromFile: bad dimensions in %s", path);
        free(buf);
        return false;
    }

    LayeredMap_Shutdown(m);
    if (!LayeredMap_Init(m, width, height, tile_size))
    {
        free(buf);
        return false;
    }

    const int n = width * height;

    // Read sections
    bool got_ground = false, got_deco = false, got_coll = false, got_interact = false;

    while (1)
    {
        char* tok = next_token(&cursor);
        if (!tok) break;

        char key[32];
        SDL_strlcpy(key, tok, sizeof(key));
        str_to_upper(key);

        if (token_is(key, "GROUND"))
        {
            got_ground = read_ints(&cursor, m->ground, n);
        }
        else if (token_is(key, "DECO"))
        {
            got_deco = read_ints(&cursor, m->deco, n);
        }
        else if (token_is(key, "COLLISION") || token_is(key, "COLL"))
        {
            got_coll = read_ints(&cursor, m->coll, n);
        }
        else if (token_is(key, "INTERACT"))
        {
            got_interact = read_ints(&cursor, m->interact, n);
        }
        else
        {
            // Unknown token; ignore.
        }
    }

    free(buf);

    // Missing sections are fine; they default to 0.
    SDL_Log("Loaded map %s: %dx%d ts=%d sections: ground=%d deco=%d coll=%d interact=%d",
            path, width, height, tile_size, got_ground, got_deco, got_coll, got_interact);

    return true;
}
