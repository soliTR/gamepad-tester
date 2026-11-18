/* Fading trail: the most recent positions of a stick or finger */
#ifndef TRAIL_H
#define TRAIL_H

#include <SDL3/SDL.h>

#define TRAIL_LEN 90

typedef struct {
    float x[TRAIL_LEN], y[TRAIL_LEN];
    int head, n;
} Trail;

void trail_push(Trail *t, float x, float y);
void trail_clear(Trail *t);
/* Draws the trail as fading dots at (ox + x * sx, oy + y * sy) */
void trail_draw(const Trail *t, float ox, float oy, float sx, float sy, SDL_Color c, Uint8 max_alpha);

#endif
