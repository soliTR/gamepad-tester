/* Tab "Übersicht": Daten und Fähigkeiten des Controllers, Liste aller Controller */
#include <stdarg.h>
#include <stdio.h>

#include "eventlog.h"
#include "padinfo.h"
#include "pads.h"
#include "tabs.h"
#include "ui.h"

/* Zeile "Bezeichnung   Wert" */
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
    row(x, y, label, v ? C_GREEN : C_DIM, "%s%s", v ? "ja" : "nein", v && extra ? extra : "");
}

static float draw_controller_data(SDL_Gamepad *g, float x, float y)
{
    char buf[64];
    ui_text(x, y, 2, C_ACCENT, "Controller");
    y += 30;
    row(x, y, "Name", C_TEXT, "%.22s", SDL_GetGamepadName(g));
    y += 22;
    row(x, y, "Typ", C_TEXT, "%s", pad_type_name(SDL_GetGamepadType(g)));
    y += 22;
    row(x, y, "Hersteller/Produkt", C_TEXT, "%04x:%04x", SDL_GetGamepadVendor(g), SDL_GetGamepadProduct(g));
    y += 22;
    const char *serial = SDL_GetGamepadSerial(g);
    row(x, y, "Seriennummer", C_TEXT, "%.22s", serial && *serial ? serial : "-");
    y += 22;
    Uint16 fw = SDL_GetGamepadFirmwareVersion(g);
    if (fw) {
        row(x, y, "Firmware", C_TEXT, "%04x", fw);
    } else {
        row(x, y, "Firmware", C_TEXT, "-");
    }
    y += 22;
    row(x, y, "Verbindung", C_TEXT, "%s", pad_connection_text(g));
    y += 22;
    row(x, y, "Stromversorgung", C_TEXT, "%s", pad_power_text(g, buf, sizeof buf));
    y += 22;
    int player = SDL_GetGamepadPlayerIndex(g);
    if (player >= 0) {
        row(x, y, "Spieler-Nummer", C_TEXT, "%d", player + 1);
    } else {
        row(x, y, "Spieler-Nummer", C_TEXT, "-");
    }
    return y + 40;
}

static void draw_capabilities(SDL_Gamepad *g, float x, float y)
{
    char rate[32];
    ui_text(x, y, 2, C_ACCENT, "Fähigkeiten");
    y += 30;
    int buttons = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        buttons += SDL_GamepadHasButton(g, (SDL_GamepadButton)b);
    }
    row(x, y, "Tasten", C_TEXT, "%d", buttons);
    y += 22;
    row_yesno(x, y, "Analoge Trigger", pad_has_trigger(g, 0), NULL);
    y += 22;
    row_yesno(x, y, "Vibration", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN), NULL);
    y += 22;
    row_yesno(x, y, "Trigger-Vibration", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN), NULL);
    y += 22;
    snprintf(rate, sizeof rate, " (%.0f Hz)", SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO));
    row_yesno(x, y, "Gyrosensor", SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO), rate);
    y += 22;
    snprintf(rate, sizeof rate, " (%.0f Hz)", SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_ACCEL));
    row_yesno(x, y, "Beschleunigung", SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL), rate);
    y += 22;
    row_yesno(x, y, "Touchpad", SDL_GetNumGamepadTouchpads(g) > 0, NULL);
    y += 22;
    row_yesno(x, y, "RGB-LED", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN), NULL);
    y += 22;
    row_yesno(x, y, "Spieler-LED", pad_has_cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN), NULL);
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    float y = draw_controller_data(g, 30, 116);
    draw_capabilities(g, 30, y);

    float x = 720;
    y = 116;
    ui_text(x, y, 2, C_ACCENT, "Verbundene Controller (anklicken)");
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
    ui_text(x, y, 2, C_ACCENT, "Ereignisse");
    eventlog_draw(x, y + 30, 12, 33);
}

static void reset(void) {}

const Tab tab_info = {"Übersicht", reset, NULL, draw, NULL};
