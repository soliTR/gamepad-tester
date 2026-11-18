/* "Extras" tab: touchpad, LED color, player LED and battery */
#include <math.h>
#include <stdio.h>

#include "eventlog.h"
#include "padinfo.h"
#include "tabs.h"
#include "trail.h"
#include "ui.h"

#define MAX_FINGERS 2

static Trail touch_trails[MAX_FINGERS];
static bool rainbow;
static Uint64 rainbow_last;
static SDL_Color led_color = {0, 0, 255, 255};

static void reset(void)
{
    for (int f = 0; f < MAX_FINGERS; f++) {
        trail_clear(&touch_trails[f]);
    }
    rainbow = false;
}

static void reset_key(void)
{
    for (int f = 0; f < MAX_FINGERS; f++) {
        trail_clear(&touch_trails[f]);
    }
}

/* Converts a hue of 0..1 into a saturated RGB color */
static SDL_Color hue_to_color(float h)
{
    float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
    return (SDL_Color){(Uint8)(SDL_clamp(r, 0.0f, 1.0f) * 255), (Uint8)(SDL_clamp(g, 0.0f, 1.0f) * 255),
                       (Uint8)(SDL_clamp(b, 0.0f, 1.0f) * 255), 255};
}

static void update(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    if (SDL_GetNumGamepadTouchpads(g) > 0) {
        int fingers = SDL_min(SDL_GetNumGamepadTouchpadFingers(g, 0), MAX_FINGERS);
        for (int f = 0; f < fingers; f++) {
            bool down;
            float x, y, pressure;
            if (SDL_GetGamepadTouchpadFinger(g, 0, f, &down, &x, &y, &pressure) && down) {
                trail_push(&touch_trails[f], x, y);
            }
        }
    }

    Uint64 now = SDL_GetTicks();
    if (rainbow && now - rainbow_last > 40) { /* don't set the LED more than about 25 times per second */
        rainbow_last = now;
        led_color = hue_to_color((float)(now % 3000) / 3000.0f);
        SDL_SetGamepadLED(g, led_color.r, led_color.g, led_color.b);
    }
}

static void draw_touchpad(SDL_Gamepad *g)
{
    ui_fill(16, 104, 620, 400, C_PANEL);
    ui_text(30, 114, 2, C_ACCENT, "Touchpad");
    if (SDL_GetNumGamepadTouchpads(g) == 0) {
        ui_text(30, 150, 2, C_DIM, "No touchpad available.");
        return;
    }
    const float x = 46, y = 160, w = 560, h = 280;
    const SDL_Color finger_colors[MAX_FINGERS] = {C_ACCENT, C_PRESS};
    ui_fill(x, y, w, h, C_BG);
    ui_rect(x, y, w, h, C_LINE);
    for (int f = 0; f < MAX_FINGERS; f++) {
        trail_draw(&touch_trails[f], x, y, w, h, finger_colors[f], 200);
    }
    int fingers = SDL_min(SDL_GetNumGamepadTouchpadFingers(g, 0), MAX_FINGERS);
    for (int f = 0; f < fingers; f++) {
        bool down;
        float fx, fy, pressure;
        if (SDL_GetGamepadTouchpadFinger(g, 0, f, &down, &fx, &fy, &pressure) && down) {
            ui_disc(x + fx * w, y + fy * h, 16, finger_colors[f]);
            ui_textf(x + 8, y + h + 12 + f * 22, 2, finger_colors[f], "Finger %d: x %.2f  y %.2f", f + 1, fx, fy);
        }
    }
    if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_TOUCHPAD)) {
        ui_rect(x - 3, y - 3, w + 6, h + 6, C_PRESS);
    }
}

static void draw_battery(SDL_Gamepad *g)
{
    char buf[64];
    ui_fill(16, 514, 620, 246, C_PANEL);
    ui_text(30, 524, 2, C_ACCENT, "Battery / connection");
    ui_text(30, 556, 2, C_TEXT, pad_power_text(g, buf, sizeof buf));
    ui_textf(30, 580, 2, C_TEXT, "Connection: %s", pad_connection_text(g));
}

static float draw_led(SDL_Gamepad *g, float x, float y)
{
    static const struct {
        const char *name;
        SDL_Color color;
    } presets[] = {
        {"Red", {255, 0, 0, 255}},        {"Green", {0, 255, 0, 255}},   {"Blue", {0, 0, 255, 255}},
        {"White", {255, 255, 255, 255}},  {"Yellow", {255, 200, 0, 255}}, {"Purple", {180, 0, 255, 255}},
        {"Off", {0, 0, 0, 255}},
    };
    bool has_rgb = pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN);
    ui_text(x, y, 2, C_ACCENT, has_rgb ? "Light color (RGB LED)" : "Light color: no RGB LED");
    y += 34;
    if (!has_rgb) {
        return y;
    }
    for (int i = 0; i < (int)SDL_arraysize(presets); i++) {
        float bx = x + (i % 4) * 148, by = y + (i / 4) * 46;
        if (ui_button(bx, by, 144, 38, presets[i].name, false)) {
            rainbow = false;
            led_color = presets[i].color;
            if (!SDL_SetGamepadLED(g, led_color.r, led_color.g, led_color.b)) {
                eventlog_add("LED: %s", SDL_GetError());
            }
        }
        ui_fill(bx + 4, by + 32, 136, 3, presets[i].color);
    }
    if (ui_button(x + 3 * 148, y + 46, 144, 38, "Rainbow", rainbow)) {
        rainbow = !rainbow;
    }
    y += 104;
    ui_fill(x, y, 592, 20, led_color);
    return y + 40;
}

static void draw_player_led(SDL_Gamepad *g, float x, float y)
{
    bool has_player_led = pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN);
    ui_text(x, y, 2, C_ACCENT, has_player_led ? "Player LED" : "Player LED: not reported");
    y += 34;
    int player = SDL_GetGamepadPlayerIndex(g);
    for (int i = 0; i < 4; i++) {
        char label[16];
        snprintf(label, sizeof label, "%d", i + 1);
        if (ui_button(x + i * 148, y, 144, 38, label, player == i)) {
            SDL_SetGamepadPlayerIndex(g, i);
        }
    }
    ui_text(x, y + 70, 2, C_DIM, "Extra buttons: see the Buttons tab.");
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    draw_touchpad(g);
    draw_battery(g);
    ui_fill(648, 104, 616, 656, C_PANEL);
    float y = draw_led(g, 660, 114);
    draw_player_led(g, 660, y);
}

const Tab tab_extras = {"Extras", reset, update, draw, reset_key};
