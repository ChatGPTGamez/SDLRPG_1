#pragma once
#include <stdbool.h>

typedef struct SDL_Renderer SDL_Renderer;

bool UIText_Init(SDL_Renderer* renderer);
void UIText_Shutdown(void);

bool UIText_MeasureLine(const char* text, int* out_w, int* out_h);
bool UIText_DrawLine(SDL_Renderer* renderer, float x, float y, const char* text);
