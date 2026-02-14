// src/ui/ui_text.h
#pragma once
#include <stdbool.h>

typedef struct SDL_Renderer SDL_Renderer;

bool UIText_Init(SDL_Renderer* renderer);
void UIText_Shutdown(void);

// Draws a single line. Returns true if drawn.
bool UIText_DrawLine(SDL_Renderer* renderer, float x, float y, const char* text);

// Measures a line (pixels). Returns false if not initialized.
bool UIText_MeasureLine(const char* text, int* out_w, int* out_h);
