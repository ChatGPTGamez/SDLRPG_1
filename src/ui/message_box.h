// src/ui/message_box.h
#pragma once
#include <stdbool.h>

typedef struct MessageBox
{
    bool visible;
    char text[256];
} MessageBox;

void MessageBox_Init(MessageBox* mb);
void MessageBox_Show(MessageBox* mb, const char* text);
void MessageBox_Hide(MessageBox* mb);
void MessageBox_Toggle(MessageBox* mb);

// Renders the message box + text + close hint
void MessageBox_Render(const MessageBox* mb, void* sdl_renderer, int screen_w, int screen_h);
