/* Tab "Tasten": Controller-Grafik und Checkliste aller Tasten */
#include <stdio.h>
#include <string.h>

#include "eventlog.h"
#include "padinfo.h"
#include "tabs.h"
#include "ui.h"

/* Trigger gilt als getestet, wenn er so weit durchgedrückt wurde */
#define TRIGGER_FULL 0.95f

static bool seen[SDL_GAMEPAD_BUTTON_COUNT];
static bool seen_trigger[2];

static void reset(void)
{
    memset(seen, 0, sizeof seen);
    memset(seen_trigger, 0, sizeof seen_trigger);
}

static void reset_key(void)
{
    reset();
    eventlog_add("Checkliste zurückgesetzt");
}

void buttons_on_press(SDL_Gamepad *g, SDL_GamepadButton b)
{
    if (b >= 0 && b < SDL_GAMEPAD_BUTTON_COUNT) {
        seen[b] = true;
        eventlog_add("%s gedrückt", pad_button_name(g, b));
    }
}

static void update(SDL_Gamepad *g, const Input *in)
{
    for (int s = 0; s < 2; s++) {
        if (in->trigger[s] >= TRIGGER_FULL && !seen_trigger[s]) {
            seen_trigger[s] = true;
            eventlog_add("%s voll durchgedrückt", pad_trigger_name(g, s));
        }
    }
}

/* ------------------------------------------------------------------ */
/* Controller-Grafik                                                   */
/* ------------------------------------------------------------------ */

static SDL_Color button_color(SDL_Gamepad *g, SDL_GamepadButton b)
{
    if (SDL_GetGamepadButton(g, b)) {
        return C_PRESS;
    }
    return seen[b] ? C_GREEN_DIM : C_PANEL3;
}

static void draw_button_rect(SDL_Gamepad *g, SDL_GamepadButton b, float x, float y, float w, float h, const char *label)
{
    if (!SDL_GamepadHasButton(g, b)) {
        return;
    }
    ui_fill(x, y, w, h, button_color(g, b));
    ui_rect(x, y, w, h, C_LINE);
    if (label) {
        ui_text_center(x + w / 2, y + (h - 8) / 2, 1, C_TEXT, label);
    }
}

/* Runde Haupttaste; PlayStation-Symbole werden gezeichnet, sonst die Aufschrift */
static void draw_face_button(SDL_Gamepad *g, SDL_GamepadButton b, float x, float y)
{
    if (!SDL_GamepadHasButton(g, b)) {
        return;
    }
    ui_disc(x, y, 18, button_color(g, b));
    ui_circle(x, y, 18, C_LINE);
    switch (SDL_GetGamepadButtonLabel(g, b)) {
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        ui_line(x - 7, y - 7, x + 7, y + 7, C_TEXT);
        ui_line(x - 7, y + 7, x + 7, y - 7, C_TEXT);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        ui_circle(x, y, 8, C_TEXT);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        ui_rect(x - 7, y - 7, 14, 14, C_TEXT);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        ui_line(x, y - 8, x - 8, y + 6, C_TEXT);
        ui_line(x - 8, y + 6, x + 8, y + 6, C_TEXT);
        ui_line(x + 8, y + 6, x, y - 8, C_TEXT);
        break;
    default:
        ui_text_center(x, y - 8, 2, C_TEXT, pad_button_name(g, b));
        break;
    }
}

static void draw_stick(SDL_Gamepad *g, const Input *in, int s, float cx, float cy)
{
    SDL_GamepadButton click = s ? SDL_GAMEPAD_BUTTON_RIGHT_STICK : SDL_GAMEPAD_BUTTON_LEFT_STICK;
    ui_disc(cx, cy, 42, C_PANEL);
    ui_circle(cx, cy, 42, seen[click] ? C_GREEN : C_LINE);
    ui_disc(cx + in->x[s] * 30, cy + in->y[s] * 30, 12, SDL_GetGamepadButton(g, click) ? C_PRESS : C_TEXT);
}

static void draw_trigger(SDL_Gamepad *g, const Input *in, int s, float x, float y, float w, float h)
{
    if (!pad_has_trigger(g, s)) {
        return;
    }
    float v = in->trigger[s];
    ui_fill(x, y, w, h, C_PANEL);
    ui_fill(x, y, w * v, h, v > 0.02f ? C_PRESS : C_PANEL3);
    ui_rect(x, y, w, h, seen_trigger[s] ? C_GREEN : C_LINE);
    ui_textf(x + 6, y + (h - 16) / 2, 2, C_TEXT, "%s %3.0f%%", pad_trigger_name(g, s), v * 100);
}

static void draw_dpad(SDL_Gamepad *g, float cx, float cy)
{
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_DPAD_UP, cx - 13, cy - 39, 26, 26, NULL);
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN, cx - 13, cy + 13, 26, 26, NULL);
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT, cx - 39, cy - 13, 26, 26, NULL);
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, cx + 13, cy - 13, 26, 26, NULL);
    ui_fill(cx - 13, cy - 13, 26, 26, C_PANEL3);
}

static void draw_controller(SDL_Gamepad *g, const Input *in)
{
    /* PlayStation: Steuerkreuz oben links, linker Stick unten. Sonst umgekehrt. */
    bool ps = pad_is_playstation(SDL_GetGamepadType(g));

    /* Gehäuse */
    ui_disc(410, 290, 80, C_PANEL2);
    ui_disc(870, 290, 80, C_PANEL2);
    ui_disc(450, 440, 80, C_PANEL2);
    ui_disc(830, 440, 80, C_PANEL2);
    ui_fill(410, 210, 460, 230, C_PANEL2);

    draw_trigger(g, in, 0, 400, 126, 160, 28);
    draw_trigger(g, in, 1, 720, 126, 160, 28);
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 400, 164, 160, 26, pad_button_name(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 720, 164, 160, 26, pad_button_name(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));

    draw_stick(g, in, 0, ps ? 560 : 470, ps ? 390 : 290);
    draw_stick(g, in, 1, 720, 390);
    draw_dpad(g, ps ? 470 : 560, ps ? 290 : 390);

    draw_face_button(g, SDL_GAMEPAD_BUTTON_NORTH, 810, 250);
    draw_face_button(g, SDL_GAMEPAD_BUTTON_SOUTH, 810, 330);
    draw_face_button(g, SDL_GAMEPAD_BUTTON_WEST, 770, 290);
    draw_face_button(g, SDL_GAMEPAD_BUTTON_EAST, 850, 290);

    if (SDL_GamepadHasButton(g, SDL_GAMEPAD_BUTTON_TOUCHPAD)) {
        draw_button_rect(g, SDL_GAMEPAD_BUTTON_TOUCHPAD, 575, 212, 130, 60, "Touchpad");
        draw_button_rect(g, SDL_GAMEPAD_BUTTON_BACK, 538, 218, 28, 18, NULL);
        draw_button_rect(g, SDL_GAMEPAD_BUTTON_START, 714, 218, 28, 18, NULL);
    } else {
        draw_button_rect(g, SDL_GAMEPAD_BUTTON_BACK, 585, 250, 34, 20, NULL);
        draw_button_rect(g, SDL_GAMEPAD_BUTTON_START, 661, 250, 34, 20, NULL);
    }
    if (SDL_GamepadHasButton(g, SDL_GAMEPAD_BUTTON_GUIDE)) {
        ui_disc(640, 308, 14, button_color(g, SDL_GAMEPAD_BUTTON_GUIDE));
        ui_circle(640, 308, 14, C_LINE);
    }
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_MISC1, 628, 332, 24, 12, NULL);
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_MISC2, 628, 350, 24, 12, NULL);

    draw_button_rect(g, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, 420, 540, 100, 22, "Paddle L1");
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, 420, 568, 100, 22, "Paddle L2");
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, 760, 540, 100, 22, "Paddle R1");
    draw_button_rect(g, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, 760, 568, 100, 22, "Paddle R2");
    for (int i = 0; i < 4; i++) {
        char label[8];
        snprintf(label, sizeof label, "Z%d", i + 3);
        draw_button_rect(g, (SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_MISC3 + i), 560 + i * 42, 600, 36, 22, label);
    }
}

/* ------------------------------------------------------------------ */
/* Checkliste                                                          */
/* ------------------------------------------------------------------ */

static void check_item(float x, float y, bool done, bool pressed, const char *label)
{
    ui_fill(x, y, 16, 16, pressed ? C_PRESS : done ? C_GREEN : C_PANEL2);
    ui_rect(x, y, 16, 16, C_LINE);
    if (done) {
        ui_line(x + 3, y + 8, x + 7, y + 12, C_BG);
        ui_line(x + 7, y + 12, x + 13, y + 4, C_BG);
    }
    ui_text(x + 24, y, 2, done ? C_TEXT : C_DIM, label);
}

/* Zeichnet die Checkliste und gibt zurück, ob alles getestet ist */
static bool draw_checklist(SDL_Gamepad *g, const Input *in, float x, float y)
{
    int total = 0, done = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        if (SDL_GamepadHasButton(g, (SDL_GamepadButton)b)) {
            total++;
            done += seen[b];
        }
    }
    for (int s = 0; s < 2; s++) {
        if (pad_has_trigger(g, s)) {
            total++;
            done += seen_trigger[s];
        }
    }
    ui_textf(x, y, 2, done == total ? C_GREEN : C_ACCENT, "Getestet: %d / %d", done, total);
    y += 28;

    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        SDL_GamepadButton btn = (SDL_GamepadButton)b;
        if (SDL_GamepadHasButton(g, btn)) {
            check_item(x, y, seen[b], SDL_GetGamepadButton(g, btn), pad_button_name(g, btn));
            y += 22;
        }
    }
    for (int s = 0; s < 2; s++) {
        if (pad_has_trigger(g, s)) {
            char label[32];
            snprintf(label, sizeof label, "%s ganz drücken", pad_trigger_name(g, s));
            check_item(x, y, seen_trigger[s], in->trigger[s] > 0.02f, label);
            y += 22;
        }
    }
    return total > 0 && done == total;
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    draw_controller(g, in);
    bool complete = draw_checklist(g, in, 950, 112);

    ui_text(20, 112, 2, C_ACCENT, "Letzte Eingaben");
    eventlog_draw(20, 140, 24, 19);
    ui_text(20, 660, 2, C_PRESS, "orange = gedrückt");
    ui_text(20, 682, 2, C_GREEN, "grün = getestet");
    ui_text(20, 726, 2, C_DIM, "[R] zurücksetzen");
    if (complete) {
        ui_text_center(640, 684, 3, C_GREEN, "Alle Tasten getestet!");
    }
}

const Tab tab_buttons = {"Tasten", reset, update, draw, reset_key};
