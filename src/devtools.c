#include "devtools.h"

#include <math.h>

#include "pads.h"
#include "ui.h"

#define SHOT_AFTER_FRAMES 30
#define RECORD_EVERY_NTH_FRAME 3 /* 60 Hz display -> 20 frames per second */

static bool virtual_enabled;
static const char *shot_path;
static const char *record_dir;
static float record_seconds;
static int frames;
static int recorded;

static bool SDLCALL virtual_rumble(void *userdata, Uint16 low, Uint16 high)
{
    (void)userdata;
    (void)low;
    (void)high;
    return true;
}

static bool SDLCALL virtual_led(void *userdata, Uint8 r, Uint8 g, Uint8 b)
{
    (void)userdata;
    (void)r;
    (void)g;
    (void)b;
    return true;
}

/* Virtual controller with all buttons, a touchpad and sensors */
static void attach_virtual_pad(void)
{
    static SDL_VirtualJoystickTouchpadDesc touchpad = {2, {0, 0, 0}};
    static SDL_VirtualJoystickSensorDesc sensors[2] = {{SDL_SENSOR_GYRO, 250}, {SDL_SENSOR_ACCEL, 250}};
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.ntouchpads = 1;
    desc.touchpads = &touchpad;
    desc.nsensors = 2;
    desc.sensors = sensors;
    desc.name = "Virtual test controller";
    desc.Rumble = virtual_rumble;
    desc.SetLED = virtual_led;
    if (!SDL_AttachVirtualJoystick(&desc)) {
        SDL_Log("Virtual controller failed: %s", SDL_GetError());
    }
}

void devtools_init(void)
{
    shot_path = SDL_getenv("GAMEPAD_TESTER_SHOT");
    record_dir = SDL_getenv("GAMEPAD_TESTER_RECORD");
    const char *seconds = SDL_getenv("GAMEPAD_TESTER_RECORD_SECONDS");
    record_seconds = seconds ? (float)SDL_atof(seconds) : 4.0f;
    virtual_enabled = SDL_getenv("GAMEPAD_TESTER_VIRTUAL") != NULL;
    if (virtual_enabled) {
        attach_virtual_pad();
    }
}

int devtools_start_tab(void)
{
    const char *tab = SDL_getenv("GAMEPAD_TESTER_TAB");
    return tab ? SDL_atoi(tab) : -1;
}

/*
 * Demo input: the left stick circles, the right stick rests slightly off center
 * (like a drifting stick), the buttons are pressed one after another, the
 * triggers move up and down, a finger moves on the touchpad and the gyro turns.
 */
void devtools_before_frame(void)
{
    SDL_Gamepad *g = pads_current();
    if (!virtual_enabled || !g || !SDL_IsJoystickVirtual(SDL_GetGamepadID(g))) {
        return;
    }
    SDL_Joystick *j = SDL_GetGamepadJoystick(g);
    float t = (float)SDL_GetTicks() / 1000.0f;
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, (Sint16)(cosf(t * 3) * 32000));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, (Sint16)(sinf(t * 3) * 32000));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTX, (Sint16)(900 + sinf(t * 7) * 250));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTY, (Sint16)(-1400 + cosf(t * 5) * 250));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, (Sint16)((sinf(t * 2) * 0.5f + 0.5f) * 32767));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, (Sint16)((cosf(t * 1.5f) * 0.5f + 0.5f) * 32767));
    int pressed = (int)(t * 5) % SDL_GAMEPAD_BUTTON_COUNT;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        SDL_SetJoystickVirtualButton(j, b, b == pressed);
    }
    SDL_SetJoystickVirtualTouchpad(j, 0, 0, true, 0.5f + 0.3f * cosf(t * 1.5f), 0.5f + 0.3f * sinf(t * 3), 1.0f);
    float gyro[3] = {0.4f * sinf(t * 2), 0.6f, 0.2f};
    float accel[3] = {0.5f, 9.7f, -0.8f};
    Uint64 ns = SDL_GetTicksNS();
    SDL_SendJoystickVirtualSensorData(j, SDL_SENSOR_GYRO, ns, gyro, 3);
    SDL_SendJoystickVirtualSensorData(j, SDL_SENSOR_ACCEL, ns, accel, 3);
}

static void save_frame(const char *path)
{
    SDL_Surface *s = SDL_RenderReadPixels(ui_renderer(), NULL);
    if (s) {
        SDL_SaveBMP(s, path);
        SDL_DestroySurface(s);
    }
}

bool devtools_after_draw(void)
{
    frames++;
    if (record_dir) {
        if (frames % RECORD_EVERY_NTH_FRAME == 0) {
            char path[1024];
            SDL_snprintf(path, sizeof path, "%s/frame%04d.bmp", record_dir, recorded++);
            save_frame(path);
        }
        return frames >= record_seconds * 60;
    }
    if (!shot_path || frames < SHOT_AFTER_FRAMES) {
        return false;
    }
    save_frame(shot_path);
    return true;
}
