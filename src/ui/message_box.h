#pragma once
#include <stdbool.h>

typedef struct SDL_Renderer SDL_Renderer;

typedef struct MessageBox
{
    bool open;
    char text[512];
} MessageBox;

void MessageBox_Open(MessageBox* mb, const char* text);
void MessageBox_Close(MessageBox* mb);
bool MessageBox_IsOpen(const MessageBox* mb);

void MessageBox_Render(MessageBox* mb, SDL_Renderer* r, int screen_w, int screen_h);
