/* Stick- und Triggerwerte des aktuellen Controllers, einmal pro Frame gelesen */
#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>

typedef struct {
    /* Index 0 = links, 1 = rechts */
    float x[2], y[2];     /* -1..1, Y positiv = unten (wie SDL) */
    Sint16 raw_x[2], raw_y[2];
    float trigger[2];     /* 0..1 */
} Input;

void input_read(SDL_Gamepad *g, Input *in);

#endif
