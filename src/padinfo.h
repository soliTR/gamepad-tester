/* Human-readable names and capabilities of a controller */
#ifndef PADINFO_H
#define PADINFO_H

#include <SDL3/SDL.h>

bool pad_is_playstation(SDL_GamepadType t);
bool pad_is_nintendo(SDL_GamepadType t);
const char *pad_type_name(SDL_GamepadType t);

/* Name of a button matching the controller, e.g. "Cross", "L1" or "Plus (+)" */
const char *pad_button_name(SDL_Gamepad *g, SDL_GamepadButton b);
/* "L2"/"R2", "ZL"/"ZR" or "LT"/"RT" */
const char *pad_trigger_name(SDL_Gamepad *g, int right);
bool pad_has_trigger(SDL_Gamepad *g, int right);

/* Queries an SDL_PROP_GAMEPAD_CAP_* property */
bool pad_has_cap(SDL_Gamepad *g, const char *prop);

const char *pad_power_text(SDL_Gamepad *g, char *buf, size_t n);
const char *pad_connection_text(SDL_Gamepad *g);

#endif
