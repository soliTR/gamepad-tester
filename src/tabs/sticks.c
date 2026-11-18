/* Tab "Sticks": beide Sticks live mit Leuchtspur, Trigger als Balken */
#include <math.h>

#include "padinfo.h"
#include "tabs.h"
#include "trail.h"
#include "ui.h"

#define RAD2DEG (180.0f / SDL_PI_F)

static Trail trails[2];

static void reset(void)
{
    trail_clear(&trails[0]);
    trail_clear(&trails[1]);
}

static void update(SDL_Gamepad *g, const Input *in)
{
    (void)g;
    for (int s = 0; s < 2; s++) {
        trail_push(&trails[s], in->x[s], in->y[s]);
    }
}

static bool stick_clicked(SDL_Gamepad *g, int s)
{
    return SDL_GetGamepadButton(g, s ? SDL_GAMEPAD_BUTTON_RIGHT_STICK : SDL_GAMEPAD_BUTTON_LEFT_STICK);
}

static void draw_stick_view(SDL_Gamepad *g, const Input *in, int s, float cx, float cy, float r)
{
    ui_disc(cx, cy, r, C_PANEL);
    ui_circle(cx, cy, r, C_LINE);
    ui_circle(cx, cy, r * 0.1f, C_PANEL3);
    ui_line(cx - r, cy, cx + r, cy, C_PANEL3);
    ui_line(cx, cy - r, cx, cy + r, C_PANEL3);
    trail_draw(&trails[s], cx, cy, r, r, C_ACCENT, 255);
    float px = cx + in->x[s] * r, py = cy + in->y[s] * r;
    ui_line(cx, cy, px, py, C_DIM);
    ui_disc(px, py, 10, stick_clicked(g, s) ? C_PRESS : C_TEXT);
}

static void draw_stick_values(SDL_Gamepad *g, const Input *in, int s, float x, float y)
{
    float magnitude = hypotf(in->x[s], in->y[s]);
    float angle = atan2f(-in->y[s], in->x[s]) * RAD2DEG; /* 0° = rechts, gegen den Uhrzeigersinn */
    bool click = stick_clicked(g, s);
    ui_textf(x, y, 2, C_TEXT, "X: %+6.3f  (roh %6d)", in->x[s], in->raw_x[s]);
    ui_textf(x, y + 22, 2, C_TEXT, "Y: %+6.3f  (roh %6d)", in->y[s], in->raw_y[s]);
    ui_textf(x, y + 44, 2, C_TEXT, "Auslenkung: %5.1f %%", magnitude * 100);
    ui_textf(x, y + 66, 2, C_TEXT, "Winkel:     %5.0f°", angle < 0 ? angle + 360 : angle);
    ui_textf(x, y + 88, 2, click ? C_PRESS : C_DIM, "Stick-Klick: %s", click ? "gedrückt" : "-");
}

static void draw_trigger_bar(SDL_Gamepad *g, const Input *in, int s, float x)
{
    const float y = 180, w = 40, h = 320;
    float v = in->trigger[s];
    ui_fill(x, y, w, h, C_PANEL);
    ui_fill(x, y + h * (1 - v), w, h * v, C_PRESS);
    ui_rect(x, y, w, h, C_LINE);
    ui_text_center(x + w / 2, y - 26, 2, C_TEXT, pad_trigger_name(g, s));
    ui_textf(x - 4, y + h + 10, 2, C_TEXT, "%3.0f%%", v * 100);
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    const char *names[2] = {"Linker Stick", "Rechter Stick"};
    const float centers[2] = {290, 990};
    const float radius = 190;
    for (int s = 0; s < 2; s++) {
        float cx = centers[s];
        ui_text_center(cx, 112, 2, C_ACCENT, names[s]);
        draw_stick_view(g, in, s, cx, 340, radius);
        draw_stick_values(g, in, s, cx - radius, 550);
        if (pad_has_trigger(g, s)) {
            draw_trigger_bar(g, in, s, s ? 680 : 560);
        }
    }
    ui_text_center(640, 600, 2, C_DIM, "Kreis innen = 10 %");
    ui_text_center(640, 622, 2, C_DIM, "Y+ = unten (SDL)");
}

const Tab tab_sticks = {"Sticks", reset, update, draw, reset};
