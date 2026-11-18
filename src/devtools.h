/*
 * Hilfen zum Testen ohne echte Hardware, gesteuert über Umgebungsvariablen:
 *
 *   GAMEPAD_TESTER_VIRTUAL=1      virtuellen Controller anhängen
 *   GAMEPAD_TESTER_TAB=<n>        mit Tab n starten (0 = Übersicht)
 *   GAMEPAD_TESTER_SHOT=<datei>   nach 30 Frames Screenshot (BMP) speichern und beenden
 */
#ifndef DEVTOOLS_H
#define DEVTOOLS_H

#include <SDL3/SDL.h>

void devtools_init(void);
/* gewünschter Start-Tab oder -1 */
int devtools_start_tab(void);
/* vor jedem Frame: bewegt den virtuellen Controller */
void devtools_before_frame(void);
/* nach dem Zeichnen, vor SDL_RenderPresent; true = Programm soll enden */
bool devtools_after_draw(void);

#endif
