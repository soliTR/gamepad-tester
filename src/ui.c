#include "ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SDL_Renderer *ren;

static float mouse_x, mouse_y;
static bool mouse_down, mouse_clicked;

SDL_Color ui_rating_color(int rating)
{
    return rating == 0 ? C_GREEN : rating == 1 ? C_YELLOW : C_RED;
}

void ui_init(SDL_Renderer *renderer)
{
    ren = renderer;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}

SDL_Renderer *ui_renderer(void) { return ren; }

/* ------------------------------------------------------------------ */
/* Mouse                                                               */
/* ------------------------------------------------------------------ */

void ui_begin_frame(void) { mouse_clicked = false; }

void ui_handle_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION:
        mouse_x = e->motion.x;
        mouse_y = e->motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button == SDL_BUTTON_LEFT) {
            mouse_x = e->button.x;
            mouse_y = e->button.y;
            mouse_down = mouse_clicked = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e->button.button == SDL_BUTTON_LEFT) {
            mouse_down = false;
        }
        break;
    default:
        break;
    }
}

static bool hover(float x, float y, float w, float h)
{
    return mouse_x >= x && mouse_x < x + w && mouse_y >= y && mouse_y < y + h;
}

/* ------------------------------------------------------------------ */
/* Shapes                                                              */
/* ------------------------------------------------------------------ */

static void set_color(SDL_Color c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); }

void ui_clear(SDL_Color c)
{
    set_color(c);
    SDL_RenderClear(ren);
}

void ui_fill(float x, float y, float w, float h, SDL_Color c)
{
    SDL_FRect r = {x, y, w, h};
    set_color(c);
    SDL_RenderFillRect(ren, &r);
}

void ui_rect(float x, float y, float w, float h, SDL_Color c)
{
    SDL_FRect r = {x, y, w, h};
    set_color(c);
    SDL_RenderRect(ren, &r);
}

void ui_line(float x1, float y1, float x2, float y2, SDL_Color c)
{
    set_color(c);
    SDL_RenderLine(ren, x1, y1, x2, y2);
}

void ui_circle(float cx, float cy, float r, SDL_Color c)
{
    set_color(c);
    float px = cx + r, py = cy;
    for (int i = 1; i <= 64; i++) {
        float a = i * 2 * SDL_PI_F / 64;
        float nx = cx + r * cosf(a), ny = cy + r * sinf(a);
        SDL_RenderLine(ren, px, py, nx, ny);
        px = nx;
        py = ny;
    }
}

void ui_disc(float cx, float cy, float r, SDL_Color c)
{
    set_color(c);
    for (float dy = -r; dy <= r; dy += 1) {
        float hw = sqrtf(r * r - dy * dy);
        SDL_RenderLine(ren, cx - hw, cy + dy, cx + hw, cy + dy);
    }
}

void ui_dot(float x, float y, SDL_Color c, Uint8 alpha)
{
    SDL_FRect p = {x - 1, y - 1, 3, 3};
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, alpha);
    SDL_RenderFillRect(ren, &p);
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

/*
 * SDL's built-in 8x8 font only knows ASCII. A few common non-ASCII
 * characters are drawn anyway (e.g. in controller names): umlauts as base
 * letter plus two dots, the degree sign as a small square, others are
 * replaced by ASCII.
 */
float ui_text(float x, float y, float s, SDL_Color c, const char *str)
{
    set_color(c);
    SDL_SetRenderScale(ren, s, s);
    float cx = x / s, cy = y / s;
    const char *p = str;
    while (*p) {
        Uint32 cp = SDL_StepUTF8(&p, NULL);
        char ch[2] = {0, 0};
        const char *rep = NULL;
        int dots = 0; /* 1 = lowercase, 2 = uppercase */
        switch (cp) {
        case 0xE4: ch[0] = 'a'; dots = 1; break;
        case 0xF6: ch[0] = 'o'; dots = 1; break;
        case 0xFC: ch[0] = 'u'; dots = 1; break;
        case 0xC4: ch[0] = 'A'; dots = 2; break;
        case 0xD6: ch[0] = 'O'; dots = 2; break;
        case 0xDC: ch[0] = 'U'; dots = 2; break;
        case 0xDF: rep = "ss"; break;
        case 0x2013: case 0x2014: rep = "-"; break;
        case 0x2026: rep = "..."; break;
        case 0x2192: rep = "->"; break;
        case 0xB0: { /* degree sign */
            SDL_FRect d = {cx + 2, cy, 3, 3};
            SDL_RenderRect(ren, &d);
            cx += 8;
            continue;
        }
        default: ch[0] = cp < 128 ? (char)cp : '?'; break;
        }
        if (rep) {
            SDL_RenderDebugText(ren, cx, cy, rep);
            cx += 8 * (float)strlen(rep);
            continue;
        }
        SDL_RenderDebugText(ren, cx, cy, ch);
        if (dots) {
            float dy = dots == 2 ? cy - 2 : cy;
            SDL_FRect d1 = {cx + 1, dy, 1, 1}, d2 = {cx + 4, dy, 1, 1};
            SDL_RenderFillRect(ren, &d1);
            SDL_RenderFillRect(ren, &d2);
        }
        cx += 8;
    }
    SDL_SetRenderScale(ren, 1, 1);
    return cx * s - x;
}

float ui_text_width(float s, const char *str)
{
    int n = 0;
    const char *p = str;
    while (*p) {
        Uint32 cp = SDL_StepUTF8(&p, NULL);
        n += cp == 0xDF ? 2 : cp == 0x2026 ? 3 : cp == 0x2192 ? 2 : 1;
    }
    return n * 8 * s;
}

float ui_textf(float x, float y, float s, SDL_Color c, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return ui_text(x, y, s, c, buf);
}

void ui_text_center(float cx, float y, float s, SDL_Color c, const char *str)
{
    ui_text(cx - ui_text_width(s, str) / 2, y, s, c, str);
}

/* ------------------------------------------------------------------ */
/* Controls                                                            */
/* ------------------------------------------------------------------ */

bool ui_button(float x, float y, float w, float h, const char *label, bool on)
{
    bool hv = hover(x, y, w, h);
    ui_fill(x, y, w, h, on ? C_ACCENT : hv ? C_PANEL3 : C_PANEL2);
    ui_rect(x, y, w, h, C_LINE);
    ui_text(x + (w - ui_text_width(2, label)) / 2, y + (h - 16) / 2, 2, on ? C_BG : C_TEXT, label);
    return hv && mouse_clicked;
}

void ui_slider(float x, float y, float w, float *v)
{
    if (mouse_down && hover(x - 8, y - 8, w + 16, 36)) {
        *v = SDL_clamp((mouse_x - x) / w, 0.0f, 1.0f);
    }
    ui_fill(x, y + 6, w, 8, C_PANEL2);
    ui_fill(x, y + 6, w * *v, 8, C_ACCENT);
    ui_disc(x + w * *v, y + 10, 10, C_TEXT);
    ui_textf(x + w + 20, y + 2, 2, C_TEXT, "%3.0f %%", *v * 100);
}
