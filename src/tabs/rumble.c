/* "Rumble" tab: drive the motors individually and in patterns */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "padinfo.h"
#include "tabs.h"
#include "ui.h"

typedef enum {
    RUMBLE_OFF,
    RUMBLE_LOW,       /* left motor (low frequency) */
    RUMBLE_HIGH,      /* right motor (high frequency) */
    RUMBLE_BOTH,
    RUMBLE_CONTINUOUS,
    RUMBLE_ALTERNATE, /* left/right alternating */
    RUMBLE_RAMP,      /* slowly getting stronger */
    RUMBLE_TRIGGER_L,
    RUMBLE_TRIGGER_R,
    RUMBLE_TRIGGER_BOTH,
} RumbleMode;

enum { MOTOR_LOW, MOTOR_HIGH, MOTOR_TRIGGER_L, MOTOR_TRIGGER_R, MOTOR_COUNT };

static RumbleMode mode;
static Uint64 mode_start;
static float strength = 0.7f;
static Uint16 current[MOTOR_COUNT]; /* last values sent */
static Uint64 last_send;
static char error[160];

static void start(RumbleMode m)
{
    mode = m;
    mode_start = SDL_GetTicks();
    error[0] = 0;
}

static void reset(void)
{
    mode = RUMBLE_OFF;
    memset(current, 0, sizeof current);
    error[0] = 0;
}

static void reset_key(void) { start(RUMBLE_OFF); }

/* Computes the motor levels for the current time; switches off expired patterns */
static void compute_levels(Uint64 now, Uint16 v[MOTOR_COUNT])
{
    float t = (float)(now - mode_start) / 1000.0f;
    Uint16 full = (Uint16)(strength * 65535);
    float duration = 1.5f;
    memset(v, 0, MOTOR_COUNT * sizeof v[0]);
    switch (mode) {
    case RUMBLE_LOW: v[MOTOR_LOW] = full; break;
    case RUMBLE_HIGH: v[MOTOR_HIGH] = full; break;
    case RUMBLE_BOTH: v[MOTOR_LOW] = v[MOTOR_HIGH] = full; break;
    case RUMBLE_CONTINUOUS: v[MOTOR_LOW] = v[MOTOR_HIGH] = full; duration = INFINITY; break;
    case RUMBLE_ALTERNATE: v[((int)(t / 0.4f)) % 2] = full; duration = 4.0f; break;
    case RUMBLE_RAMP: v[MOTOR_LOW] = v[MOTOR_HIGH] = (Uint16)(SDL_min(t / 3.0f, 1.0f) * full); duration = 3.5f; break;
    case RUMBLE_TRIGGER_L: v[MOTOR_TRIGGER_L] = full; break;
    case RUMBLE_TRIGGER_R: v[MOTOR_TRIGGER_R] = full; break;
    case RUMBLE_TRIGGER_BOTH: v[MOTOR_TRIGGER_L] = v[MOTOR_TRIGGER_R] = full; break;
    default: break;
    }
    if (mode != RUMBLE_OFF && t > duration) {
        mode = RUMBLE_OFF;
    }
    if (mode == RUMBLE_OFF) {
        memset(v, 0, MOTOR_COUNT * sizeof v[0]);
    }
}

/*
 * Only sends on changes, and additionally every second while rumbling,
 * because each command ends by itself after 2 s.
 */
static void update(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    Uint64 now = SDL_GetTicks();
    Uint16 v[MOTOR_COUNT];
    compute_levels(now, v);

    bool changed = memcmp(v, current, sizeof v) != 0;
    bool active = v[0] || v[1] || v[2] || v[3];
    if (!changed && !(active && now - last_send > 1000)) {
        return;
    }
    last_send = now;
    bool motors_involved = v[MOTOR_LOW] || v[MOTOR_HIGH] || current[MOTOR_LOW] || current[MOTOR_HIGH];
    bool triggers_involved = v[MOTOR_TRIGGER_L] || v[MOTOR_TRIGGER_R] || current[MOTOR_TRIGGER_L] || current[MOTOR_TRIGGER_R];
    if (motors_involved && !SDL_RumbleGamepad(g, v[MOTOR_LOW], v[MOTOR_HIGH], 2000)) {
        snprintf(error, sizeof error, "Rumble failed: %s", SDL_GetError());
    }
    if (triggers_involved && !SDL_RumbleGamepadTriggers(g, v[MOTOR_TRIGGER_L], v[MOTOR_TRIGGER_R], 2000)) {
        snprintf(error, sizeof error, "Trigger rumble failed: %s", SDL_GetError());
    }
    memcpy(current, v, sizeof v);
}

/* Circle that grows and wobbles with the motor level */
static void draw_motor(float x, float y, const char *label, Uint16 value)
{
    float f = value / 65535.0f;
    float wobble = f > 0 ? sinf((float)SDL_GetTicks() / 25.0f) * 4 * f : 0;
    char buf[16];
    ui_disc(x + wobble, y, 30 + f * 30, f > 0 ? C_PRESS : C_PANEL2);
    ui_circle(x, y, 60, C_LINE);
    ui_text_center(x, y + 70, 2, C_TEXT, label);
    snprintf(buf, sizeof buf, "%3.0f %%", f * 100);
    ui_text_center(x, y + 92, 2, C_DIM, buf);
}

/* Button that starts a pattern and stays highlighted while it runs */
static void mode_button(float x, float y, float w, const char *label, RumbleMode m)
{
    if (ui_button(x, y, w, 40, label, mode == m)) {
        start(m);
    }
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    bool has_rumble = pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN);
    bool has_trigger_rumble = pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN);
    float x = 40, y = 116;

    if (!has_rumble) {
        ui_text(x, y, 2, C_YELLOW, "No rumble reported - you can still try.");
        y += 30;
    }
    ui_text(x, y, 2, C_TEXT, "Strength");
    ui_slider(x + 150, y - 2, 400, &strength);
    y += 50;

    mode_button(x, y, 310, "Left (low) 1.5 s", RUMBLE_LOW);
    mode_button(x + 330, y, 310, "Right (high) 1.5 s", RUMBLE_HIGH);
    y += 54;
    mode_button(x, y, 310, "Both 1.5 s", RUMBLE_BOTH);
    if (ui_button(x + 330, y, 310, 40, mode == RUMBLE_CONTINUOUS ? "Continuous OFF" : "Continuous ON",
                  mode == RUMBLE_CONTINUOUS)) {
        start(mode == RUMBLE_CONTINUOUS ? RUMBLE_OFF : RUMBLE_CONTINUOUS);
    }
    y += 54;
    mode_button(x, y, 310, "Alternate L/R (4 s)", RUMBLE_ALTERNATE);
    mode_button(x + 330, y, 310, "Ramp 0-100 % (3 s)", RUMBLE_RAMP);
    y += 54;
    if (ui_button(x, y, 640, 40, "Stop", false)) {
        start(RUMBLE_OFF);
    }
    y += 70;

    ui_text(x, y, 2, has_trigger_rumble ? C_ACCENT : C_DIM,
            has_trigger_rumble ? "Trigger rumble" : "Trigger rumble: not available");
    y += 30;
    if (has_trigger_rumble) {
        mode_button(x, y, 200, "Left trigger", RUMBLE_TRIGGER_L);
        mode_button(x + 210, y, 200, "Right trigger", RUMBLE_TRIGGER_R);
        mode_button(x + 420, y, 200, "Both", RUMBLE_TRIGGER_BOTH);
        y += 54;
    } else if (SDL_GetGamepadType(g) == SDL_GAMEPAD_TYPE_PS5) {
        ui_text(x, y, 2, C_DIM, "SDL can't drive the adaptive triggers.");
        y += 24;
    }
    if (error[0]) {
        ui_text(x, y + 10, 2, C_RED, error);
    }

    draw_motor(850, 300, "Left (low)", current[MOTOR_LOW]);
    draw_motor(1110, 300, "Right (high)", current[MOTOR_HIGH]);
    if (has_trigger_rumble) {
        draw_motor(850, 560, "Trigger L", current[MOTOR_TRIGGER_L]);
        draw_motor(1110, 560, "Trigger R", current[MOTOR_TRIGGER_R]);
    }
    ui_text_center(980, 700, 2, C_DIM, "Can you feel both motors separately?");
}

const Tab tab_rumble = {"Rumble", reset, update, draw, reset_key};
