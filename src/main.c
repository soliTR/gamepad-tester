/*
 * Gamepad-Tester: Tasten, Sticks, Stickdrift, Vibration, Gyro, Touchpad und LED
 * eines Gamepads testen. Benötigt nur SDL3 (>= 3.4).
 *
 * Diese Datei enthält Start, Hauptschleife und den Rahmen (Kopfzeile, Tab-Leiste,
 * Fußzeile). Die eigentlichen Tests liegen in src/tabs/.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

#include "devtools.h"
#include "eventlog.h"
#include "input.h"
#include "pads.h"
#include "tabs/tabs.h"
#include "ui.h"

static const Tab *const tabs[] = {&tab_info, &tab_buttons, &tab_sticks, &tab_drift, &tab_rumble, &tab_gyro, &tab_extras};
#define TAB_COUNT ((int)SDL_arraysize(tabs))

static int current_tab;

static void reset_all_tabs(void)
{
    for (int i = 0; i < TAB_COUNT; i++) {
        tabs[i]->reset();
    }
}

static void set_hints(void)
{
    /* Gamepads auch dann lesen, wenn das Fenster nicht im Vordergrund ist */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    /* DualSense/DS4: erweiterte Berichte für Gyro, Touchpad, Vibration und LED */
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
    /* Switch-2-Controller laufen über libusb (der Hint fehlt noch in den SDL-Headern) */
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_SWITCH2", "1");
    SDL_SetHint(SDL_HINT_HIDAPI_LIBUSB, "1");
}

/* ------------------------------------------------------------------ */
/* Ereignisse                                                          */
/* ------------------------------------------------------------------ */

static void handle_key(SDL_Keycode key, bool *running)
{
    if (key == SDLK_ESCAPE) {
        *running = false;
    } else if (key >= SDLK_1 && key < SDLK_1 + TAB_COUNT) {
        current_tab = (int)(key - SDLK_1);
    } else if (key == SDLK_RIGHT) {
        current_tab = (current_tab + 1) % TAB_COUNT;
    } else if (key == SDLK_LEFT) {
        current_tab = (current_tab + TAB_COUNT - 1) % TAB_COUNT;
    } else if (key == SDLK_N) {
        pads_select_next();
    } else if (key == SDLK_R && tabs[current_tab]->reset_key) {
        tabs[current_tab]->reset_key();
    }
}

static void handle_event(const SDL_Event *e, bool *running)
{
    SDL_Gamepad *g = pads_current();
    SDL_JoystickID current_id = g ? SDL_GetGamepadID(g) : 0;
    ui_handle_event(e);
    switch (e->type) {
    case SDL_EVENT_QUIT: *running = false; break;
    case SDL_EVENT_KEY_DOWN: handle_key(e->key.key, running); break;
    case SDL_EVENT_GAMEPAD_ADDED: pads_add(e->gdevice.which); break;
    case SDL_EVENT_GAMEPAD_REMOVED: pads_remove(e->gdevice.which); break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (g && e->gbutton.which == current_id) {
            buttons_on_press(g, (SDL_GamepadButton)e->gbutton.button);
        }
        break;
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        if (g && e->gsensor.which == current_id) {
            gyro_on_sensor((SDL_SensorType)e->gsensor.sensor, e->gsensor.data, e->gsensor.sensor_timestamp);
        }
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* Rahmen                                                              */
/* ------------------------------------------------------------------ */

static void draw_header(SDL_Gamepad *g)
{
    ui_text(16, 14, 3, C_TEXT, "Gamepad-Tester");
    if (g) {
        char buf[96];
        snprintf(buf, sizeof buf, "%.40s", SDL_GetGamepadName(g));
        ui_text(SCREEN_W - 16 - ui_text_width(2, buf), 10, 2, C_ACCENT, buf);
        snprintf(buf, sizeof buf, "Controller %d von %d%s", pads_current_index() + 1, pads_count(),
                 pads_count() > 1 ? "  [N] wechseln" : "");
        ui_text(SCREEN_W - 16 - ui_text_width(2, buf), 32, 2, C_DIM, buf);
    }
    for (int i = 0; i < TAB_COUNT; i++) {
        float x = 10 + i * 181;
        if (ui_button(x, 58, 175, 36, tabs[i]->name, i == current_tab)) {
            current_tab = i;
        }
        ui_textf(x + 4, 61, 1, i == current_tab ? C_BG : C_DIM, "%d", i + 1);
    }
}

static void draw_footer(void)
{
    ui_fill(0, SCREEN_H - 30, SCREEN_W, 30, C_PANEL);
    ui_text(16, SCREEN_H - 23, 2, C_DIM, "1-7 / Pfeiltasten: Tab   N: Controller   R: zurücksetzen   Esc: Beenden");
}

static void draw_no_pad(void)
{
    ui_text_center(640, 300, 3, C_TEXT, "Kein Controller verbunden");
    ui_text_center(640, 360, 2, C_DIM, "Schließe jetzt einen Controller per USB an");
    ui_text_center(640, 384, 2, C_DIM, "oder verbinde ihn per Bluetooth.");
    ui_text_center(640, 430, 2, C_DIM, "Switch 2 Pro Controller: nur per USB-Kabel.");
    if (eventlog_count()) {
        ui_text_center(640, 500, 2, C_ACCENT, "Ereignisse");
        eventlog_draw(440, 530, 6, 40);
    }
}

/* Ein Frame: Werte lesen, alle Tabs aktualisieren, sichtbaren Tab zeichnen */
static bool run_frame(SDL_Renderer *ren)
{
    SDL_Gamepad *g = pads_current();
    Input in = {0};
    if (g) {
        input_read(g, &in);
        for (int i = 0; i < TAB_COUNT; i++) {
            if (tabs[i]->update) {
                tabs[i]->update(g, &in);
            }
        }
    }

    ui_clear(C_BG);
    draw_header(g);
    if (!g) {
        draw_no_pad();
    } else {
        tabs[current_tab]->draw(g, &in);
    }
    draw_footer();

    bool quit = devtools_after_draw();
    SDL_RenderPresent(ren);
    return !quit;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    set_hints();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init fehlgeschlagen: %s", SDL_GetError());
        return 1;
    }
    SDL_Window *win;
    SDL_Renderer *ren;
    if (!SDL_CreateWindowAndRenderer("Gamepad-Tester", SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE, &win, &ren)) {
        SDL_Log("Fenster fehlgeschlagen: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderLogicalPresentation(ren, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(ren, 1);
    ui_init(ren);
    pads_on_change(reset_all_tabs);
    eventlog_add("Programm gestartet");

    devtools_init();
    int start_tab = devtools_start_tab();
    if (start_tab >= 0) {
        current_tab = SDL_min(start_tab, TAB_COUNT - 1);
    }

    bool running = true;
    while (running) {
        ui_begin_frame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL_ConvertEventToRenderCoordinates(ren, &e);
            handle_event(&e, &running);
        }
        devtools_before_frame();
        if (!run_frame(ren)) {
            running = false;
        }
    }

    pads_close_all();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
