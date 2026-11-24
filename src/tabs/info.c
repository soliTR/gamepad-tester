/* "Overview" tab: controller data and capabilities, list of all controllers */
#include <stdarg.h>
#include <stdio.h>

#include "eventlog.h"
#include "padinfo.h"
#include "pads.h"
#include "pollrate.h"
#include "tabs.h"
#include "ui.h"

#define GRAPH_Y 570

/* "Label   value" row */
static void row(float x, float y, const char *label, SDL_Color c, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
    SDL_PRINTF_VARARG_FUNC(5);

static void row(float x, float y, const char *label, SDL_Color c, const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ui_text(x, y, 2, C_DIM, label);
    ui_text(x + 300, y, 2, c, buf);
}

static void row_yesno(float x, float y, const char *label, bool v, const char *extra)
{
    row(x, y, label, v ? C_GREEN : C_DIM, "%s%s", v ? "yes" : "no", v && extra ? extra : "");
}

static float draw_controller_data(SDL_Gamepad *g, float x, float y)
{
    char buf[64];
    ui_text(x, y, 2, C_ACCENT, "Controller");
    y += 30;
    row(x, y, "Name", C_TEXT, "%.22s", SDL_GetGamepadName(g));
    y += 22;
    row(x, y, "Type", C_TEXT, "%s", pad_type_name(SDL_GetGamepadType(g)));
    y += 22;
    row(x, y, "Vendor/product", C_TEXT, "%04x:%04x", SDL_GetGamepadVendor(g), SDL_GetGamepadProduct(g));
    y += 22;
    const char *serial = SDL_GetGamepadSerial(g);
    row(x, y, "Serial number", C_TEXT, "%.22s", serial && *serial ? serial : "-");
    y += 22;
    Uint16 fw = SDL_GetGamepadFirmwareVersion(g);
    if (fw) {
        row(x, y, "Firmware", C_TEXT, "%04x", fw);
    } else {
        row(x, y, "Firmware", C_TEXT, "-");
    }
    y += 22;
    row(x, y, "Connection", C_TEXT, "%s", pad_connection_text(g));
    y += 22;
    PollRate pr;
    if (pollrate_get(SDL_GetTicksNS(), &pr)) {
        int nominal = pollrate_nominal(pr.hz);
        if (nominal) {
            row(x, y, "Polling rate", C_TEXT, "%.0f Hz (~%d Hz)", pr.hz, nominal);
        } else {
            row(x, y, "Polling rate", C_TEXT, "%.0f Hz", pr.hz);
        }
        y += 22;
        row(x, y, "Jitter", C_TEXT, "%.2f ms", pr.jitter_ms);
    } else {
        row(x, y, "Polling rate", C_DIM, "move a stick");
        y += 22;
        row(x, y, "Jitter", C_DIM, "-");
    }
    y += 22;
    row(x, y, "Power", C_TEXT, "%s", pad_power_text(g, buf, sizeof buf));
    y += 22;
    int player = SDL_GetGamepadPlayerIndex(g);
    if (player >= 0) {
        row(x, y, "Player number", C_TEXT, "%d", player + 1);
    } else {
        row(x, y, "Player number", C_TEXT, "-");
    }
    return y + 40;
}

static void draw_capabilities(SDL_Gamepad *g, float x, float y)
{
    char rate[32];
    ui_text(x, y, 2, C_ACCENT, "Capabilities");
    y += 30;
    int buttons = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        buttons += SDL_GamepadHasButton(g, (SDL_GamepadButton)b);
    }
    row(x, y, "Buttons", C_TEXT, "%d", buttons);
    y += 22;
    row_yesno(x, y, "Analog triggers", pad_has_trigger(g, 0), NULL);
    y += 22;
    row_yesno(x, y, "Rumble", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN), NULL);
    y += 22;
    row_yesno(x, y, "Trigger rumble", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN), NULL);
    y += 22;
    snprintf(rate, sizeof rate, " (%.0f Hz)", SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO));
    row_yesno(x, y, "Gyroscope", SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO), rate);
    y += 22;
    snprintf(rate, sizeof rate, " (%.0f Hz)", SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_ACCEL));
    row_yesno(x, y, "Accelerometer", SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL), rate);
    y += 22;
    row_yesno(x, y, "Touchpad", SDL_GetNumGamepadTouchpads(g) > 0, NULL);
    y += 22;
    row_yesno(x, y, "RGB LED", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN), NULL);
    y += 22;
    row_yesno(x, y, "Player LED", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN), NULL);
}

/* Live graph of the polling rate with its heading; gaps where nothing was measured */
static void draw_rate_graph(float x, float y, float w, float h)
{
    ui_text(x, y, 2, C_ACCENT, "Polling rate");
    y += 30;
    float hist[POLLRATE_HISTORY_LEN];
    int n = pollrate_history(hist, POLLRATE_HISTORY_LEN);
    float peak = 0;
    for (int i = 0; i < n; i++) {
        peak = SDL_max(peak, hist[i]);
    }
    /* Scale up to the next standard rate so the line does not jump around */
    int scale = 125;
    while (scale < 8000 && peak > scale * 1.05f) {
        scale *= 2;
    }
    ui_fill(x, y, w, h, C_PANEL);
    /* Reference line at the standard rate */
    float ref_y = y + h - 4 - (h - 8) / 1.05f;
    ui_line(x, ref_y, x + w, ref_y, C_PANEL3);
    float px = 0, py = 0;
    bool have_prev = false;
    for (int i = 0; i < n; i++) {
        if (hist[i] <= 0) {
            have_prev = false;
            continue;
        }
        /* The newest value is always at the right edge */
        float gx = x + w - (n - 1 - i) * w / (float)(POLLRATE_HISTORY_LEN - 1);
        float gy = y + h - 4 - SDL_min(hist[i] / (scale * 1.05f), 1.0f) * (h - 8);
        if (have_prev) {
            ui_line(px, py, gx, gy, C_ACCENT);
        }
        px = gx;
        py = gy;
        have_prev = true;
    }
    char label[32];
    snprintf(label, sizeof label, "0-%d Hz, 10 s", scale);
    ui_text(x + w - ui_text_width(2, label), y - 30, 2, C_DIM, label);
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    float y = draw_controller_data(g, 30, 116);
    draw_capabilities(g, 30, y);

    float x = 720;
    y = 116;
    ui_text(x, y, 2, C_ACCENT, "Connected controllers (click)");
    y += 30;
    for (int i = 0; i < pads_count(); i++) {
        char label[64];
        snprintf(label, sizeof label, "%d: %.30s", i + 1, SDL_GetGamepadName(pads_get(i)));
        bool selected = i == pads_current_index();
        if (ui_button(x, y, 544, 30, label, selected) && !selected) {
            pads_select(i);
            return;
        }
        y += 36;
    }
    y += 20;
    ui_text(x, y, 2, C_ACCENT, "Events");
    /* As many lines as fit above the graph */
    int lines = SDL_clamp((int)((GRAPH_Y - 20 - (y + 30)) / 20), 0, 12);
    eventlog_draw(x, y + 30, lines, 33);

    draw_rate_graph(x, GRAPH_Y, 544, 150);
}

static void reset(void) {}

const Tab tab_info = {"Overview", reset, NULL, draw, NULL};
