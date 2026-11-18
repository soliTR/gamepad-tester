/* Event log: the most recent messages (buttons, connections, test results) */
#ifndef EVENTLOG_H
#define EVENTLOG_H

#include <SDL3/SDL.h>

void eventlog_add(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);
int eventlog_count(void);
/* Draws the newest messages first, each cut to maxchars characters */
void eventlog_draw(float x, float y, int lines, int maxchars);

#endif
