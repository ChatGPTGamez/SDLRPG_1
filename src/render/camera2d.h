#pragma once
#include <stdbool.h>

typedef struct Camera2D
{
    // Camera position in WORLD coordinates (top-left of the view in world space)
    float x;
    float y;

    // Viewport size in pixels (screen size)
    int viewport_w;
    int viewport_h;
} Camera2D;

void Camera2D_SetViewport(Camera2D* c, int w, int h);

// Center camera on a world point (cx, cy)
void Camera2D_CenterOn(Camera2D* c, float cx, float cy);

// World -> screen transform
void Camera2D_WorldToScreen(const Camera2D* c, float wx, float wy, float* sx, float* sy);
