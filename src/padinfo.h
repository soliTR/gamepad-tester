/* Namen und Fähigkeiten eines Controllers in lesbarer Form */
#ifndef PADINFO_H
#define PADINFO_H

#include <SDL3/SDL.h>

bool pad_is_playstation(SDL_GamepadType t);
bool pad_is_nintendo(SDL_GamepadType t);
const char *pad_type_name(SDL_GamepadType t);

/* Name einer Taste passend zum Controller, z. B. "Kreuz", "L1" oder "Plus (+)" */
const char *pad_button_name(SDL_Gamepad *g, SDL_GamepadButton b);
/* "L2"/"R2", "ZL"/"ZR" oder "LT"/"RT" */
const char *pad_trigger_name(SDL_Gamepad *g, int right);
bool pad_has_trigger(SDL_Gamepad *g, int right);

/* Fragt eine SDL_PROP_GAMEPAD_CAP_*-Eigenschaft ab */
bool pad_has_cap(SDL_Gamepad *g, const char *prop);

const char *pad_power_text(SDL_Gamepad *g, char *buf, size_t n);
const char *pad_connection_text(SDL_Gamepad *g);

#endif
