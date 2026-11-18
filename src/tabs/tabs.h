/*
 * Common interface of the tabs. Each tab lives in its own file and keeps
 * its own test state.
 */
#ifndef TABS_H
#define TABS_H

#include <SDL3/SDL.h>

#include "input.h"

typedef struct {
    const char *name;
    /* Clear the test state because a different controller was selected */
    void (*reset)(void);
    /* Every frame, even when the tab is not visible (e.g. ongoing rumble). May be NULL. */
    void (*update)(SDL_Gamepad *g, const Input *in);
    void (*draw)(SDL_Gamepad *g, const Input *in);
    /* R key in the visible tab. May be NULL. */
    void (*reset_key)(void);
} Tab;

extern const Tab tab_info;
extern const Tab tab_buttons;
extern const Tab tab_sticks;
extern const Tab tab_drift;
extern const Tab tab_rumble;
extern const Tab tab_gyro;
extern const Tab tab_extras;

/* Events that only individual tabs need */
void buttons_on_press(SDL_Gamepad *g, SDL_GamepadButton b);
void gyro_on_sensor(SDL_SensorType type, const float *data, Uint64 timestamp_ns);

#endif
