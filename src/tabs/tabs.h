/*
 * Gemeinsame Schnittstelle der Tabs. Jeder Tab liegt in einer eigenen Datei
 * und hält seinen Teststatus selbst.
 */
#ifndef TABS_H
#define TABS_H

#include <SDL3/SDL.h>

#include "input.h"

typedef struct {
    const char *name;
    /* Teststatus löschen, weil ein anderer Controller ausgewählt wurde */
    void (*reset)(void);
    /* Jeden Frame, auch wenn der Tab nicht sichtbar ist (z. B. laufende Vibration). Darf NULL sein. */
    void (*update)(SDL_Gamepad *g, const Input *in);
    void (*draw)(SDL_Gamepad *g, const Input *in);
    /* Taste R im sichtbaren Tab. Darf NULL sein. */
    void (*reset_key)(void);
} Tab;

extern const Tab tab_info;
extern const Tab tab_buttons;
extern const Tab tab_sticks;
extern const Tab tab_drift;
extern const Tab tab_rumble;
extern const Tab tab_gyro;
extern const Tab tab_extras;

/* Ereignisse, die nur einzelne Tabs brauchen */
void buttons_on_press(SDL_Gamepad *g, SDL_GamepadButton b);
void gyro_on_sensor(SDL_SensorType type, const float *data, Uint64 timestamp_ns);

#endif
