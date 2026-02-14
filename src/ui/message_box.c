#include "ui/message_box.h"
#include "ui/ui_text.h"

#include <SDL3/SDL.h>
#include <string.h>

void MessageBox_Open(MessageBox* mb, const char* text)
{
    if (!mb) return;
    mb->open = true;
    mb->text[0] = '\0';
    if (text)
    {
        strncpy(mb->text, text, sizeof(mb->text) - 1);
        mb->text[sizeof(mb->text) - 1] = '\0';
    }
}

void MessageBox_Close(MessageBox* mb)
{
    if (!mb) return;
    mb->open = false;
}

bool MessageBox_IsOpen(const MessageBox* mb)
{
    return mb && mb->open;
}

static void fill(SDL_Renderer* r, SDL_FRect rc, Uint8 rr, Uint8 gg, Uint8 bb, Uint8 aa)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, rr, gg, bb, aa);
    SDL_RenderFillRect(r, &rc);
}

static void outline(SDL_Renderer* r, SDL_FRect rc, Uint8 rr, Uint8 gg, Uint8 bb, Uint8 aa)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, rr, gg, bb, aa);
    SDL_RenderRect(r, &rc);
}

void MessageBox_Render(MessageBox* mb, SDL_Renderer* r, int screen_w, int screen_h)
{
    if (!mb || !mb->open) return;

    (void)UIText_Init(r);

    const float pad = 18.0f;
    const float box_h = 150.0f;

    SDL_FRect dim = { 0, 0, (float)screen_w, (float)screen_h };
    fill(r, dim, 0, 0, 0, 90);

    SDL_FRect box = {
        pad,
        (float)screen_h - box_h - pad,
        (float)screen_w - pad * 2.0f,
        box_h
    };

    fill(r, box, 10, 10, 12, 215);
    outline(r, box, 200, 200, 200, 180);

    const float tx = box.x + 18.0f;
    const float ty = box.y + 18.0f;

    if (mb->text[0])
        UIText_DrawLine(r, tx, ty, mb->text);

    UIText_DrawLine(r, tx, box.y + box.h - 34.0f, "E / Esc: Close");
}
