#include "input.h"

/* Maps the raw value -32768..32767 to -1..1 so that both ends reach exactly 1 */
static float normalize(Sint16 v) { return v < 0 ? v / 32768.0f : v / 32767.0f; }

void input_read(SDL_Gamepad *g, Input *in)
{
    in->raw_x[0] = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX);
    in->raw_y[0] = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY);
    in->raw_x[1] = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX);
    in->raw_y[1] = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY);
    for (int s = 0; s < 2; s++) {
        in->x[s] = normalize(in->raw_x[s]);
        in->y[s] = normalize(in->raw_y[s]);
    }
    in->trigger[0] = SDL_max(0.0f, normalize(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)));
    in->trigger[1] = SDL_max(0.0f, normalize(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)));
}
