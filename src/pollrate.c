#include "pollrate.h"

#include <math.h>

/* Enough for a full second of an 8000 Hz controller */
#define MAX_STAMPS 8192
#define WINDOW_NS SDL_NS_PER_SECOND
/* Longer pauses mean the controller sent nothing new (e.g. sticks at rest), not a slow rate */
#define MAX_GAP_NS (50 * SDL_NS_PER_MS)
#define MIN_SAMPLES 10
#define HISTORY_STEP_NS (100 * SDL_NS_PER_MS)

static Uint64 stamps[MAX_STAMPS];
static int head, count;

static float history[POLLRATE_HISTORY_LEN];
static int hist_head, hist_count;
static Uint64 hist_next_ns;

void pollrate_reset(void)
{
    head = count = 0;
    hist_head = hist_count = 0;
    hist_next_ns = 0;
}

void pollrate_add(Uint64 timestamp_ns)
{
    /* One report often produces several events (axes, buttons, sensors) with the same timestamp */
    if (count > 0) {
        Uint64 last = stamps[(head + MAX_STAMPS - 1) % MAX_STAMPS];
        if (timestamp_ns <= last) {
            return;
        }
    }
    stamps[head] = timestamp_ns;
    head = (head + 1) % MAX_STAMPS;
    if (count < MAX_STAMPS) {
        count++;
    }
}

bool pollrate_get(Uint64 now_ns, PollRate *out)
{
    double sum = 0, sum_sq = 0;
    int n = 0;
    /* Walk back from the newest timestamp until the window is left */
    for (int i = 1; i < count; i++) {
        Uint64 newer = stamps[(head + MAX_STAMPS - i) % MAX_STAMPS];
        Uint64 older = stamps[(head + MAX_STAMPS - i - 1) % MAX_STAMPS];
        if (now_ns > older && now_ns - older > WINDOW_NS) {
            break;
        }
        Uint64 gap = newer - older;
        if (gap > MAX_GAP_NS) {
            continue;
        }
        double ms = (double)gap / SDL_NS_PER_MS;
        sum += ms;
        sum_sq += ms * ms;
        n++;
    }
    if (n < MIN_SAMPLES) {
        return false;
    }
    double mean = sum / n;
    double var = sum_sq / n - mean * mean;
    out->interval_ms = (float)mean;
    out->hz = (float)(1000.0 / mean);
    out->jitter_ms = (float)sqrt(var > 0 ? var : 0);
    out->samples = n;
    return true;
}

int pollrate_nominal(float hz)
{
    static const int rates[] = {125, 250, 500, 1000, 2000, 4000, 8000};
    /* Reports without a change produce no event, so the measured rate is usually a bit lower */
    for (size_t i = 0; i < SDL_arraysize(rates); i++) {
        float ratio = hz / (float)rates[i];
        if (ratio >= 0.85f && ratio <= 1.05f) {
            return rates[i];
        }
    }
    return 0;
}

void pollrate_tick(Uint64 now_ns)
{
    if (now_ns < hist_next_ns) {
        return;
    }
    PollRate pr;
    history[hist_head] = pollrate_get(now_ns, &pr) ? pr.hz : 0;
    hist_head = (hist_head + 1) % POLLRATE_HISTORY_LEN;
    if (hist_count < POLLRATE_HISTORY_LEN) {
        hist_count++;
    }
    hist_next_ns = now_ns + HISTORY_STEP_NS;
}

int pollrate_history(float *out, int max)
{
    int n = SDL_min(hist_count, max);
    for (int i = 0; i < n; i++) {
        out[i] = history[(hist_head - n + i + POLLRATE_HISTORY_LEN) % POLLRATE_HISTORY_LEN];
    }
    return n;
}
