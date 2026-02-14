// src/ui/ui_text.c
#include "ui_text.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>

static bool g_inited = false;
static TTF_Font* g_font = NULL;

static const char* kFontTry1 = "assets/fonts/DejaVuSans.ttf";
static const char* kFontTry2 = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
static const float kFontPtSize = 20.0f;

bool UIText_Init(SDL_Renderer* renderer)
{
    (void)renderer;

    if (g_inited) return (g_font != NULL);

    if (TTF_Init() < 0)
    {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        g_inited = true;
        return false;
    }

    g_font = TTF_OpenFont(kFontTry1, kFontPtSize);
    if (!g_font)
        g_font = TTF_OpenFont(kFontTry2, kFontPtSize);

    if (!g_font)
    {
        SDL_Log("TTF_OpenFont failed. Put a font at %s or install DejaVuSans. SDL error: %s",
                kFontTry1, SDL_GetError());
        g_inited = true;
        return false;
    }

    g_inited = true;
    SDL_Log("UIText_Init OK (font loaded)");
    return true;
}

void UIText_Shutdown(void)
{
    if (g_font)
    {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    if (g_inited)
    {
        TTF_Quit();
        g_inited = false;
    }
}

bool UIText_MeasureLine(const char* text, int* out_w, int* out_h)
{
    if (!g_font || !text || !text[0]) return false;

    // SDL3_ttf removed TTF_SizeText; measure by rendering a surface and reading w/h.
    SDL_Color fg = { 255, 255, 255, 255 };
    SDL_Surface* s = TTF_RenderText_Blended(g_font, text, 0, fg); // length=0 => null-terminated
    if (!s) return false;

    if (out_w) *out_w = s->w;
    if (out_h) *out_h = s->h;

    SDL_DestroySurface(s);
    return true;
}

bool UIText_DrawLine(SDL_Renderer* renderer, float x, float y, const char* text)
{
    if (!renderer || !text || !text[0]) return false;
    if (!g_font) return false;

    SDL_Color fg = { 240, 240, 240, 255 };

    // length=0 => null-terminated (SDL3_ttf API)
    SDL_Surface* s = TTF_RenderText_Blended(g_font, text, 0, fg);
    if (!s)
    {
        SDL_Log("TTF_RenderText_Blended failed: %s", SDL_GetError());
        return false;
    }

    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_DestroySurface(s);

    if (!t)
    {
        SDL_Log("CreateTextureFromSurface failed: %s", SDL_GetError());
        return false;
    }

    float tw = 0.0f, th = 0.0f;
    SDL_GetTextureSize(t, &tw, &th);

    SDL_FRect dst = { x, y, tw, th };
    SDL_RenderTexture(renderer, t, NULL, &dst);

    SDL_DestroyTexture(t);
    return true;
}
