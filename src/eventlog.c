#include "eventlog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"

#define LOG_LINES 24
#define LOG_WIDTH 64

static char lines_buf[LOG_LINES][LOG_WIDTH];
static int next, count;

void eventlog_add(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lines_buf[next], LOG_WIDTH, fmt, ap);
    va_end(ap);
    next = (next + 1) % LOG_LINES;
    if (count < LOG_LINES) {
        count++;
    }
}

int eventlog_count(void) { return count; }

/* Cuts str to at most max characters without splitting a UTF-8 character */
static void clip(char *dst, size_t n, const char *str, int max)
{
    const char *p = str;
    for (int i = 0; i < max && *p; i++) {
        SDL_StepUTF8(&p, NULL);
    }
    size_t len = SDL_min((size_t)(p - str), n - 1);
    memcpy(dst, str, len);
    dst[len] = 0;
}

void eventlog_draw(float x, float y, int lines, int maxchars)
{
    char buf[LOG_WIDTH];
    for (int i = 0; i < lines && i < count; i++) {
        int idx = (next - 1 - i + LOG_LINES) % LOG_LINES;
        clip(buf, sizeof buf, lines_buf[idx], maxchars);
        ui_text(x, y + i * 20, 2, i == 0 ? C_TEXT : C_DIM, buf);
    }
}
