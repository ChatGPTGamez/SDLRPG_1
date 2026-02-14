#include "camera2d.h"

void Camera2D_SetViewport(Camera2D* c, int w, int h)
{
    c->viewport_w = w;
    c->viewport_h = h;
}

void Camera2D_CenterOn(Camera2D* c, float cx, float cy)
{
    c->x = cx - (float)c->viewport_w * 0.5f;
    c->y = cy - (float)c->viewport_h * 0.5f;
}

void Camera2D_WorldToScreen(const Camera2D* c, float wx, float wy, float* sx, float* sy)
{
    *sx = wx - c->x;
    *sy = wy - c->y;
}
