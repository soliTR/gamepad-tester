/*
 * Helpers for testing without real hardware, controlled by environment variables:
 *
 *   GAMEPAD_TESTER_VIRTUAL=1      attach a virtual controller
 *   GAMEPAD_TESTER_TAB=<n>        start with tab n (0 = Overview)
 *   GAMEPAD_TESTER_SHOT=<file>    save a screenshot (BMP) after 30 frames and quit
 *   GAMEPAD_TESTER_RECORD=<dir>   save frames as <dir>/frameNNNN.bmp (20 per second), then quit
 *   GAMEPAD_TESTER_RECORD_SECONDS=<s>  recording length, default 4
 */
#ifndef DEVTOOLS_H
#define DEVTOOLS_H

#include <SDL3/SDL.h>

void devtools_init(void);
/* requested start tab or -1 */
int devtools_start_tab(void);
/* before every frame: moves the virtual controller */
void devtools_before_frame(void);
/* after drawing, before SDL_RenderPresent; true = the program should quit */
bool devtools_after_draw(void);

#endif
