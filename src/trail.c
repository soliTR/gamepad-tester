#include "trail.h"

#include "ui.h"

void trail_push(Trail *t, float x, float y)
{
    t->x[t->head] = x;
    t->y[t->head] = y;
    t->head = (t->head + 1) % TRAIL_LEN;
    if (t->n < TRAIL_LEN) {
        t->n++;
    }
}

void trail_clear(Trail *t) { SDL_zerop(t); }

void trail_draw(const Trail *t, float ox, float oy, float sx, float sy, SDL_Color c, Uint8 max_alpha)
{
    for (int i = 0; i < t->n; i++) {
        int idx = (t->head - 1 - i + TRAIL_LEN) % TRAIL_LEN;
        Uint8 alpha = (Uint8)(max_alpha - i * max_alpha / TRAIL_LEN);
        ui_dot(ox + t->x[idx] * sx, oy + t->y[idx] * sy, c, alpha);
    }
}
