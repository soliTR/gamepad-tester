/*
 * Liste der verbundenen Controller und welcher davon gerade getestet wird.
 */
#ifndef PADS_H
#define PADS_H

#include <SDL3/SDL.h>

/* Wird aufgerufen, wenn ein anderer (oder kein) Controller ausgewählt wird */
void pads_on_change(void (*callback)(void));

void pads_add(SDL_JoystickID id);
void pads_remove(SDL_JoystickID id);
void pads_close_all(void);

void pads_select(int index);
void pads_select_next(void);

int pads_count(void);
SDL_Gamepad *pads_get(int index);
/* aktuell getesteter Controller oder NULL */
SDL_Gamepad *pads_current(void);
int pads_current_index(void);

#endif
