/* Tab "Vibration": Motoren einzeln und in Mustern ansteuern */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "padinfo.h"
#include "tabs.h"
#include "ui.h"

typedef enum {
    RUMBLE_OFF,
    RUMBLE_LOW,       /* linker Motor (tief) */
    RUMBLE_HIGH,      /* rechter Motor (hoch) */
    RUMBLE_BOTH,
    RUMBLE_CONTINUOUS,
    RUMBLE_ALTERNATE, /* links/rechts im Wechsel */
    RUMBLE_RAMP,      /* langsam lauter */
    RUMBLE_TRIGGER_L,
    RUMBLE_TRIGGER_R,
    RUMBLE_TRIGGER_BOTH,
} RumbleMode;

enum { MOTOR_LOW, MOTOR_HIGH, MOTOR_TRIGGER_L, MOTOR_TRIGGER_R, MOTOR_COUNT };

static RumbleMode mode;
static Uint64 mode_start;
static float strength = 0.7f;
static Uint16 current[MOTOR_COUNT]; /* zuletzt gesendete Werte */
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

/* Berechnet die Motorstärken für den aktuellen Zeitpunkt; schaltet abgelaufene Muster ab */
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
 * Sendet nur bei Änderungen, während der Vibration zusätzlich jede Sekunde,
 * weil jeder Befehl nach 2 s von selbst endet.
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
        snprintf(error, sizeof error, "Vibration fehlgeschlagen: %s", SDL_GetError());
    }
    if (triggers_involved && !SDL_RumbleGamepadTriggers(g, v[MOTOR_TRIGGER_L], v[MOTOR_TRIGGER_R], 2000)) {
        snprintf(error, sizeof error, "Trigger-Vibration fehlgeschlagen: %s", SDL_GetError());
    }
    memcpy(current, v, sizeof v);
}

/* Kreis, der mit der Motorstärke wächst und wackelt */
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

/* Schaltfläche, die ein Muster startet und hervorgehoben ist, solange es läuft */
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
        ui_text(x, y, 2, C_YELLOW, "Dieser Controller meldet keine Vibration. Du kannst es trotzdem versuchen.");
        y += 30;
    }
    ui_text(x, y, 2, C_TEXT, "Stärke");
    ui_slider(x + 120, y - 2, 400, &strength);
    y += 50;

    mode_button(x, y, 310, "Links (tief) 1,5 s", RUMBLE_LOW);
    mode_button(x + 330, y, 310, "Rechts (hoch) 1,5 s", RUMBLE_HIGH);
    y += 54;
    mode_button(x, y, 310, "Beide 1,5 s", RUMBLE_BOTH);
    if (ui_button(x + 330, y, 310, 40, mode == RUMBLE_CONTINUOUS ? "Dauerbetrieb AUS" : "Dauerbetrieb AN",
                  mode == RUMBLE_CONTINUOUS)) {
        start(mode == RUMBLE_CONTINUOUS ? RUMBLE_OFF : RUMBLE_CONTINUOUS);
    }
    y += 54;
    mode_button(x, y, 310, "Wechsel L/R (4 s)", RUMBLE_ALTERNATE);
    mode_button(x + 330, y, 310, "Rampe 0-100 % (3 s)", RUMBLE_RAMP);
    y += 54;
    if (ui_button(x, y, 640, 40, "Stopp", false)) {
        start(RUMBLE_OFF);
    }
    y += 70;

    ui_text(x, y, 2, has_trigger_rumble ? C_ACCENT : C_DIM,
            has_trigger_rumble ? "Trigger-Vibration" : "Trigger-Vibration: nicht vorhanden");
    y += 30;
    if (has_trigger_rumble) {
        mode_button(x, y, 200, "Trigger links", RUMBLE_TRIGGER_L);
        mode_button(x + 210, y, 200, "Trigger rechts", RUMBLE_TRIGGER_R);
        mode_button(x + 420, y, 200, "Beide", RUMBLE_TRIGGER_BOTH);
        y += 54;
    } else if (SDL_GetGamepadType(g) == SDL_GAMEPAD_TYPE_PS5) {
        ui_text(x, y, 2, C_DIM, "Die adaptiven Trigger der DualSense steuert SDL nicht an.");
        y += 24;
    }
    if (error[0]) {
        ui_text(x, y + 10, 2, C_RED, error);
    }

    draw_motor(850, 300, "Links (tief)", current[MOTOR_LOW]);
    draw_motor(1110, 300, "Rechts (hoch)", current[MOTOR_HIGH]);
    if (has_trigger_rumble) {
        draw_motor(850, 560, "Trigger L", current[MOTOR_TRIGGER_L]);
        draw_motor(1110, 560, "Trigger R", current[MOTOR_TRIGGER_R]);
    }
    ui_text_center(980, 700, 2, C_DIM, "Spürst du beide Motoren getrennt?");
}

const Tab tab_rumble = {"Vibration", reset, update, draw, reset_key};
