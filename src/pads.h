/*
 * List of connected controllers and which one is currently being tested.
 */
#ifndef PADS_H
#define PADS_H

#include <SDL3/SDL.h>

/* Called when a different (or no) controller gets selected */
void pads_on_change(void (*callback)(void));

void pads_add(SDL_JoystickID id);
void pads_remove(SDL_JoystickID id);
void pads_close_all(void);

void pads_select(int index);
void pads_select_next(void);

int pads_count(void);
SDL_Gamepad *pads_get(int index);
/* controller currently under test, or NULL */
SDL_Gamepad *pads_current(void);
int pads_current_index(void);

#endif
