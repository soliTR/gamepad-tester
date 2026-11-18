/* Stick and trigger values of the current controller, read once per frame */
#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>

typedef struct {
    /* index 0 = left, 1 = right */
    float x[2], y[2];     /* -1..1, positive Y = down (as in SDL) */
    Sint16 raw_x[2], raw_y[2];
    float trigger[2];     /* 0..1 */
} Input;

void input_read(SDL_Gamepad *g, Input *in);

#endif
