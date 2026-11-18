#include "pads.h"

#include <string.h>

#include "eventlog.h"
#include "padinfo.h"

#define MAX_PADS 8

static SDL_Gamepad *pads[MAX_PADS];
static int count;
static int active = -1;
static void (*change_callback)(void);

void pads_on_change(void (*callback)(void)) { change_callback = callback; }

int pads_count(void) { return count; }
SDL_Gamepad *pads_get(int index) { return pads[index]; }
SDL_Gamepad *pads_current(void) { return active >= 0 ? pads[active] : NULL; }
int pads_current_index(void) { return active; }

static void stop_rumble(SDL_Gamepad *g)
{
    if (g) {
        SDL_RumbleGamepad(g, 0, 0, 0);
        SDL_RumbleGamepadTriggers(g, 0, 0, 0);
    }
}

void pads_select(int index)
{
    stop_rumble(pads_current());
    active = index;
    if (change_callback) {
        change_callback();
    }
}

void pads_select_next(void)
{
    if (count > 1) {
        pads_select((active + 1) % count);
    }
}

/* Prints the detected capabilities to the console (helps with troubleshooting) */
static void log_capabilities(SDL_Gamepad *g)
{
    SDL_Log("Connected: %s | type %s | %04x:%04x | connection %d | rumble %d, trigger rumble %d, RGB %d, "
            "player LED %d | gyro %d (%.0f Hz), accel %d | touchpads %d",
            SDL_GetGamepadName(g), pad_type_name(SDL_GetGamepadType(g)), SDL_GetGamepadVendor(g),
            SDL_GetGamepadProduct(g), (int)SDL_GetGamepadConnectionState(g),
            pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN), pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN),
            pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN), pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN),
            SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO), SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO),
            SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL), SDL_GetNumGamepadTouchpads(g));
}

void pads_add(SDL_JoystickID id)
{
    if (count >= MAX_PADS) {
        return;
    }
    SDL_Gamepad *g = SDL_OpenGamepad(id);
    if (!g) {
        eventlog_add("Error: %s", SDL_GetError());
        return;
    }
    /* Sensors only deliver data once they are enabled */
    if (SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO)) {
        SDL_SetGamepadSensorEnabled(g, SDL_SENSOR_GYRO, true);
    }
    if (SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL)) {
        SDL_SetGamepadSensorEnabled(g, SDL_SENSOR_ACCEL, true);
    }
    pads[count++] = g;
    eventlog_add("Connected: %.40s", SDL_GetGamepadName(g));
    log_capabilities(g);
    pads_select(count - 1); /* a newly plugged-in controller is tested right away */
}

void pads_remove(SDL_JoystickID id)
{
    for (int i = 0; i < count; i++) {
        if (SDL_GetGamepadID(pads[i]) != id) {
            continue;
        }
        eventlog_add("Disconnected: %.38s", SDL_GetGamepadName(pads[i]));
        SDL_Log("Disconnected: %s", SDL_GetGamepadName(pads[i]));
        SDL_CloseGamepad(pads[i]);
        memmove(&pads[i], &pads[i + 1], (size_t)(count - i - 1) * sizeof pads[0]);
        count--;
        if (i == active) {
            active = -1;
            pads_select(count > 0 ? 0 : -1);
        } else if (i < active) {
            active--;
        }
        return;
    }
}

void pads_close_all(void)
{
    stop_rumble(pads_current());
    for (int i = 0; i < count; i++) {
        SDL_CloseGamepad(pads[i]);
    }
    count = 0;
    active = -1;
}
