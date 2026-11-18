/*
 * Drawing, text and simple buttons.
 *
 * All coordinates refer to a logical area of SCREEN_W x SCREEN_H pixels;
 * SDL scales it to the window size.
 */
#ifndef UI_H
#define UI_H

#include <SDL3/SDL.h>

#define SCREEN_W 1280
#define SCREEN_H 800

static const SDL_Color C_BG = {18, 20, 26, 255};
static const SDL_Color C_PANEL = {28, 32, 42, 255};
static const SDL_Color C_PANEL2 = {42, 48, 62, 255};
static const SDL_Color C_PANEL3 = {58, 66, 86, 255};
static const SDL_Color C_LINE = {78, 88, 110, 255};
static const SDL_Color C_TEXT = {224, 228, 236, 255};
static const SDL_Color C_DIM = {132, 140, 156, 255};
static const SDL_Color C_ACCENT = {90, 165, 255, 255};
static const SDL_Color C_GREEN = {80, 200, 120, 255};
static const SDL_Color C_GREEN_DIM = {40, 100, 64, 255};
static const SDL_Color C_YELLOW = {232, 192, 60, 255};
static const SDL_Color C_RED = {232, 82, 82, 255};
static const SDL_Color C_PRESS = {255, 168, 40, 255};

/* Rating: 0 = good (green), 1 = borderline (yellow), 2 = bad (red) */
SDL_Color ui_rating_color(int rating);

void ui_init(SDL_Renderer *renderer);
SDL_Renderer *ui_renderer(void);

/* Mouse: pass events on and call ui_begin_frame() before every frame */
void ui_begin_frame(void);
void ui_handle_event(const SDL_Event *e);

void ui_clear(SDL_Color c);
void ui_fill(float x, float y, float w, float h, SDL_Color c);
void ui_rect(float x, float y, float w, float h, SDL_Color c);
void ui_line(float x1, float y1, float x2, float y2, SDL_Color c);
void ui_circle(float cx, float cy, float r, SDL_Color c);
void ui_disc(float cx, float cy, float r, SDL_Color c);
/* small dot (3x3) with its own opacity, e.g. for trails */
void ui_dot(float x, float y, SDL_Color c, Uint8 alpha);

/*
 * Text in the built-in 8x8 font, s = scale (2 = 16 px high).
 * Returns the width in pixels.
 */
float ui_text(float x, float y, float s, SDL_Color c, const char *str);
float ui_textf(float x, float y, float s, SDL_Color c, SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(5);
float ui_text_width(float s, const char *str);
void ui_text_center(float cx, float y, float s, SDL_Color c, const char *str);

/* Button; true if it was clicked in this frame. on = highlighted */
bool ui_button(float x, float y, float w, float h, const char *label, bool on);
/* Slider for *v in the range 0..1 */
void ui_slider(float x, float y, float w, float *v);

#endif
