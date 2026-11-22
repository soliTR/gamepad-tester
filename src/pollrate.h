/*
 * Polling rate: how often the controller under test sends new input, measured
 * from the timestamps of its gamepad events over the last second.
 */
#ifndef POLLRATE_H
#define POLLRATE_H

#include <SDL3/SDL.h>

typedef struct {
    float hz;          /* reports per second */
    float interval_ms; /* mean time between two reports */
    float jitter_ms;   /* standard deviation of that time */
    int samples;       /* number of intervals measured */
} PollRate;

void pollrate_reset(void);
/* Timestamp (SDL_GetTicksNS clock) of a gamepad event of the controller under test */
void pollrate_add(Uint64 timestamp_ns);
/* Fills out and returns true if there were enough reports within the last second */
bool pollrate_get(Uint64 now_ns, PollRate *out);
/* Standard USB/HID rate (125 ... 8000 Hz) the measured rate belongs to, or 0 if none fits */
int pollrate_nominal(float hz);

#endif
