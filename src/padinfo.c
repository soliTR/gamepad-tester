#include "padinfo.h"

#include <stdio.h>

bool pad_is_playstation(SDL_GamepadType t)
{
    return t == SDL_GAMEPAD_TYPE_PS3 || t == SDL_GAMEPAD_TYPE_PS4 || t == SDL_GAMEPAD_TYPE_PS5;
}

bool pad_is_nintendo(SDL_GamepadType t)
{
    return t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
           t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR ||
           t == SDL_GAMEPAD_TYPE_GAMECUBE;
}

const char *pad_type_name(SDL_GamepadType t)
{
    switch (t) {
    case SDL_GAMEPAD_TYPE_STANDARD: return "Standard-Gamepad";
    case SDL_GAMEPAD_TYPE_XBOX360: return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE: return "Xbox One / Series";
    case SDL_GAMEPAD_TYPE_PS3: return "PlayStation 3";
    case SDL_GAMEPAD_TYPE_PS4: return "PlayStation 4 (DualShock 4)";
    case SDL_GAMEPAD_TYPE_PS5: return "PlayStation 5 (DualSense)";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return "Nintendo Switch Pro";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "Joy-Con links";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "Joy-Con rechts";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Joy-Con Paar";
    case SDL_GAMEPAD_TYPE_GAMECUBE: return "GameCube";
    case SDL_GAMEPAD_TYPE_STEAM: return "Steam Controller";
    default: return "Unbekannt";
    }
}

/* Aufschrift der vier Haupttasten; SDL kennt sie je nach Controller-Typ */
static const char *face_button_name(SDL_Gamepad *g, SDL_GamepadButton b)
{
    switch (SDL_GetGamepadButtonLabel(g, b)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "Kreuz";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "Kreis";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "Quadrat";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Dreieck";
    default: break;
    }
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "Taste unten";
    case SDL_GAMEPAD_BUTTON_EAST: return "Taste rechts";
    case SDL_GAMEPAD_BUTTON_WEST: return "Taste links";
    default: return "Taste oben";
    }
}

const char *pad_button_name(SDL_Gamepad *g, SDL_GamepadButton b)
{
    SDL_GamepadType t = SDL_GetGamepadType(g);
    bool ps = pad_is_playstation(t), nin = pad_is_nintendo(t);
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH: return face_button_name(g, b);
    case SDL_GAMEPAD_BUTTON_BACK: return ps ? (t == SDL_GAMEPAD_TYPE_PS5 ? "Create" : "Share") : nin ? "Minus (-)" : "Select";
    case SDL_GAMEPAD_BUTTON_GUIDE: return ps ? "PS-Taste" : "Home";
    case SDL_GAMEPAD_BUTTON_START: return ps ? "Options" : nin ? "Plus (+)" : "Start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return ps ? "L3 (Stick-Klick)" : "L-Stick-Klick";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return ps ? "R3 (Stick-Klick)" : "R-Stick-Klick";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ps ? "L1" : nin ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ps ? "R1" : nin ? "R" : "RB";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-Pad hoch";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-Pad runter";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-Pad links";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-Pad rechts";
    case SDL_GAMEPAD_BUTTON_MISC1: return ps ? "Mikrofon" : nin ? "Capture" : "Zusatz 1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "Paddle R1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "Paddle L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "Paddle R2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "Paddle L2";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad-Klick";
    case SDL_GAMEPAD_BUTTON_MISC2: return "Zusatz 2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "Zusatz 3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "Zusatz 4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "Zusatz 5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "Zusatz 6";
    default: return "?";
    }
}

const char *pad_trigger_name(SDL_Gamepad *g, int right)
{
    SDL_GamepadType t = SDL_GetGamepadType(g);
    if (pad_is_playstation(t)) {
        return right ? "R2" : "L2";
    }
    if (pad_is_nintendo(t)) {
        return right ? "ZR" : "ZL";
    }
    return right ? "RT" : "LT";
}

bool pad_has_trigger(SDL_Gamepad *g, int right)
{
    return SDL_GamepadHasAxis(g, right ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
}

bool pad_has_cap(SDL_Gamepad *g, const char *prop)
{
    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(g), prop, false);
}

const char *pad_power_text(SDL_Gamepad *g, char *buf, size_t n)
{
    int pct = -1;
    const char *s;
    switch (SDL_GetGamepadPowerInfo(g, &pct)) {
    case SDL_POWERSTATE_ON_BATTERY: s = "Akku"; break;
    case SDL_POWERSTATE_NO_BATTERY: s = "kein Akku"; break;
    case SDL_POWERSTATE_CHARGING: s = "lädt"; break;
    case SDL_POWERSTATE_CHARGED: s = "voll geladen"; break;
    default: s = "unbekannt"; break;
    }
    if (pct >= 0) {
        snprintf(buf, n, "%s, %d %%", s, pct);
    } else {
        snprintf(buf, n, "%s", s);
    }
    return buf;
}

const char *pad_connection_text(SDL_Gamepad *g)
{
    switch (SDL_GetGamepadConnectionState(g)) {
    case SDL_JOYSTICK_CONNECTION_WIRED: return "Kabel (USB)";
    case SDL_JOYSTICK_CONNECTION_WIRELESS: return "kabellos";
    default: return "unbekannt";
    }
}
