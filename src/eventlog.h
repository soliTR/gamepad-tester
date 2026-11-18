/* Ereignis-Log: die letzten Meldungen (Tasten, Verbindungen, Testergebnisse) */
#ifndef EVENTLOG_H
#define EVENTLOG_H

#include <SDL3/SDL.h>

void eventlog_add(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);
int eventlog_count(void);
/* Zeichnet die neuesten Meldungen zuerst, jede auf maxchars Zeichen gekürzt */
void eventlog_draw(float x, float y, int lines, int maxchars);

#endif
