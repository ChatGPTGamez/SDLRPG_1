// src/ui/message_box.c
#include "message_box.h"
#include "ui_text.h"

#include <SDL3/SDL.h>
#include <string.h>

void MessageBox_Init(MessageBox* mb)
{
    if (!mb) return;
    mb->visible = false;
    mb->text[0] = '\0';
}

void MessageBox_Show(MessageBox* mb, const char* text)
{
    if (!mb) return;
    mb->visible = true;
    if (text) SDL_strlcpy(mb->text, text, sizeof(mb->text));
    else mb->text[0] = '\0';
}

void MessageBox_Hide(MessageBox* mb)
{
    if (!mb) return;
    mb->visible = false;
}

void MessageBox_Toggle(MessageBox* mb)
{
    if (!mb) return;
    mb->visible = !mb->visible;
}

void MessageBox_Render(const MessageBox* mb, void* sdl_renderer, int screen_w, int screen_h)
{
    if (!mb || !mb->visible) return;

    SDL_Renderer* r = (SDL_Renderer*)sdl_renderer;

    // Ensure text system is ready (safe to call every frame)
    (void)UIText_Init(r);

    const float pad = 24.0f;
    const float h = 150.0f;
    SDL_FRect box = { pad, (float)screen_h - h - pad, (float)screen_w - pad * 2.0f, h };

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // backdrop
    SDL_SetRenderDrawColor(r, 0, 0, 0, 215);
    SDL_RenderFillRect(r, &box);

    // border
    SDL_SetRenderDrawColor(r, 255, 255, 255, 90);
    SDL_RenderRect(r, &box);

    // Message text
    const float tx = box.x + 18.0f;
    const float ty = box.y + 18.0f;

    if (mb->text[0])
        UIText_DrawLine(r, tx, ty, mb->text);
    else
        UIText_DrawLine(r, tx, ty, "(no message)");

    // Close hint
    UIText_DrawLine(r, tx, box.y + box.h - 34.0f, "Press E or ESC to close");

    // Leave blend mode as-is; this is fine for the rest of the frame.
}
