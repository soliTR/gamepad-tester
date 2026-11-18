#include "devtools.h"

#include <math.h>

#include "pads.h"
#include "ui.h"

#define SHOT_AFTER_FRAMES 30

static bool virtual_enabled;
static const char *shot_path;
static int frames;

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

/* Virtueller Controller mit allen Tasten, Touchpad und Sensoren */
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
    desc.name = "Virtueller Test-Controller";
    desc.Rumble = virtual_rumble;
    desc.SetLED = virtual_led;
    if (!SDL_AttachVirtualJoystick(&desc)) {
        SDL_Log("Virtueller Controller fehlgeschlagen: %s", SDL_GetError());
    }
}

void devtools_init(void)
{
    shot_path = SDL_getenv("GAMEPAD_TESTER_SHOT");
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

/* Linker Stick kreist, eine Taste ist gedrückt, Sensoren melden feste Werte */
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
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTX, 900);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTY, -1400);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 20000);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_SOUTH, true);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    SDL_SetJoystickVirtualTouchpad(j, 0, 0, true, 0.3f, 0.4f, 1.0f);
    float gyro[3] = {0.4f * sinf(t * 2), 0.6f, 0.2f};
    float accel[3] = {0.5f, 9.7f, -0.8f};
    Uint64 ns = SDL_GetTicksNS();
    SDL_SendJoystickVirtualSensorData(j, SDL_SENSOR_GYRO, ns, gyro, 3);
    SDL_SendJoystickVirtualSensorData(j, SDL_SENSOR_ACCEL, ns, accel, 3);
}

bool devtools_after_draw(void)
{
    if (!shot_path || ++frames < SHOT_AFTER_FRAMES) {
        return false;
    }
    SDL_Surface *s = SDL_RenderReadPixels(ui_renderer(), NULL);
    if (s) {
        SDL_SaveBMP(s, shot_path);
        SDL_DestroySurface(s);
    }
    return true;
}
