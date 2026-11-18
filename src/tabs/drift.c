/*
 * Tab "Stickdrift"
 *
 * Ruhetest: Beide Sticks werden einige Sekunden lang nicht berührt. Die größte
 * Auslenkung in dieser Zeit zeigt, ob ein Stick von selbst aus der Mitte wandert.
 *
 * Kreistest: Der Stick wird am Rand entlang gedreht. Für jedes Winkelsegment
 * merken wir uns die größte Auslenkung und sehen so, ob der Rand überall
 * erreicht wird.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "eventlog.h"
#include "tabs.h"
#include "ui.h"

/* Grenzwerte der Drift-Ampel: maximale Auslenkung in Ruhe in Prozent */
#define DRIFT_OK 3.0f
#define DRIFT_WARN 7.0f

#define REST_WAIT_MS 3000    /* Countdown vor der Messung */
#define REST_RUN_MS 5000     /* Messdauer */
#define REST_MAX_SAMPLES 4096
#define REST_ABORT_RADIUS 0.3f /* so weit bewegt = Stick wurde angefasst */

#define CIRCLE_BINS 72       /* Winkelsegmente zu je 5 Grad */
#define CIRCLE_MIN_RADIUS 0.5f

typedef enum { REST_IDLE, REST_WAIT, REST_RUN, REST_DONE, REST_ABORTED } RestState;

typedef struct {
    float offset; /* Abstand des Mittelwerts von der Mitte, % */
    float noise;  /* größte Abweichung vom Mittelwert, % */
    float max;    /* größte Auslenkung, % */
    float mean_x, mean_y;
} RestResult;

static struct {
    RestState state;
    Uint64 t0;
    int n;
    float samples[REST_MAX_SAMPLES][2][2]; /* [Messung][Stick][x/y] */
    RestResult result[2];
} rest;

static struct {
    bool running[2];
    float radius[2][CIRCLE_BINS]; /* größte Auslenkung je Segment, 0 = noch nicht erreicht */
} circle;

static void reset(void)
{
    rest.state = REST_IDLE;
    memset(&circle, 0, sizeof circle);
}

static int drift_rating(float percent) { return percent <= DRIFT_OK ? 0 : percent <= DRIFT_WARN ? 1 : 2; }

/* ------------------------------------------------------------------ */
/* Messung                                                             */
/* ------------------------------------------------------------------ */

static void finish_rest_test(void)
{
    for (int s = 0; s < 2; s++) {
        RestResult *r = &rest.result[s];
        double sx = 0, sy = 0;
        for (int i = 0; i < rest.n; i++) {
            sx += rest.samples[i][s][0];
            sy += rest.samples[i][s][1];
        }
        r->mean_x = (float)(sx / rest.n);
        r->mean_y = (float)(sy / rest.n);
        float noise = 0, max = 0;
        for (int i = 0; i < rest.n; i++) {
            float x = rest.samples[i][s][0], y = rest.samples[i][s][1];
            noise = SDL_max(noise, hypotf(x - r->mean_x, y - r->mean_y));
            max = SDL_max(max, hypotf(x, y));
        }
        r->offset = hypotf(r->mean_x, r->mean_y) * 100;
        r->noise = noise * 100;
        r->max = max * 100;
    }
    rest.state = REST_DONE;
    eventlog_add("Ruhetest fertig (%d Messwerte)", rest.n);
}

static void update_rest_test(const Input *in, Uint64 now)
{
    if (rest.state == REST_WAIT && now - rest.t0 >= REST_WAIT_MS) {
        rest.state = REST_RUN;
        rest.t0 = now;
        rest.n = 0;
        return;
    }
    if (rest.state != REST_RUN) {
        return;
    }
    if (hypotf(in->x[0], in->y[0]) > REST_ABORT_RADIUS || hypotf(in->x[1], in->y[1]) > REST_ABORT_RADIUS) {
        rest.state = REST_ABORTED;
        return;
    }
    if (rest.n < REST_MAX_SAMPLES) {
        for (int s = 0; s < 2; s++) {
            rest.samples[rest.n][s][0] = in->x[s];
            rest.samples[rest.n][s][1] = in->y[s];
        }
        rest.n++;
    }
    if (now - rest.t0 >= REST_RUN_MS) {
        finish_rest_test();
    }
}

static void update_circle_test(const Input *in)
{
    for (int s = 0; s < 2; s++) {
        float r = hypotf(in->x[s], in->y[s]);
        if (!circle.running[s] || r <= CIRCLE_MIN_RADIUS) {
            continue;
        }
        int bin = (int)((atan2f(in->y[s], in->x[s]) + SDL_PI_F) / (2 * SDL_PI_F) * CIRCLE_BINS) % CIRCLE_BINS;
        circle.radius[s][bin] = SDL_max(circle.radius[s][bin], r);
    }
}

static void update(SDL_Gamepad *g, const Input *in)
{
    (void)g;
    update_rest_test(in, SDL_GetTicks());
    update_circle_test(in);
}

typedef struct {
    float coverage; /* erreichte Segmente, % */
    float mean;     /* mittlerer Radius, % */
    float error;    /* mittlere Abweichung vom Einheitskreis, % */
    float min;      /* kleinster Radius, % */
} CircleStats;

static CircleStats circle_stats(int s)
{
    int n = 0;
    float sum = 0, err = 0, min = 10;
    for (int i = 0; i < CIRCLE_BINS; i++) {
        float r = circle.radius[s][i];
        if (r <= 0) {
            continue;
        }
        n++;
        sum += r;
        err += fabsf(r - 1);
        min = SDL_min(min, r);
    }
    CircleStats st = {100.0f * n / CIRCLE_BINS, 0, 0, 0};
    if (n) {
        st.mean = sum / n * 100;
        st.error = err / n * 100;
        st.min = min * 100;
    }
    return st;
}

/* ------------------------------------------------------------------ */
/* Anzeige                                                             */
/* ------------------------------------------------------------------ */

/* Vergrößerte Mitte: der Rand entspricht 15 % Auslenkung */
static void draw_zoom_view(const Input *in, int s, float cx, float cy, float r)
{
    const float zoom = 15.0f;
    ui_disc(cx, cy, r, C_BG);
    ui_circle(cx, cy, r * DRIFT_OK / zoom, C_GREEN);
    ui_circle(cx, cy, r * DRIFT_WARN / zoom, C_YELLOW);
    ui_circle(cx, cy, r, C_LINE);
    ui_line(cx - r, cy, cx + r, cy, C_PANEL3);
    ui_line(cx, cy - r, cx, cy + r, C_PANEL3);
    float vx = in->x[s] * 100 / zoom, vy = in->y[s] * 100 / zoom;
    float len = hypotf(vx, vy);
    bool outside = len > 1;
    if (outside) {
        vx /= len;
        vy /= len;
    }
    ui_disc(cx + vx * r, cy + vy * r, 6, outside ? C_RED : C_TEXT);
    ui_text_center(cx, cy + r + 8, 1, C_DIM, "Zoom: Rand = 15 %");
}

static void draw_rest_result(int s, float x, float y)
{
    ui_text(x, y, 2, C_DIM, "Ruhetest");
    if (rest.state != REST_DONE) {
        ui_text(x, y + 24, 2, C_DIM, "noch nicht gemessen");
        return;
    }
    static const char *verdict[3] = {"OK - kein Drift erkennbar", "Leichter Drift (Deadzone fängt ab)",
                                     "Drift! Stick kehrt nicht zur Mitte"};
    const RestResult *r = &rest.result[s];
    int rating = drift_rating(r->max);
    ui_textf(x, y + 24, 2, C_TEXT, "Versatz:  %5.2f %%", r->offset);
    ui_textf(x, y + 46, 2, C_TEXT, "Rauschen: %5.2f %%", r->noise);
    ui_textf(x, y + 68, 2, ui_rating_color(rating), "Maximum:  %5.2f %%", r->max);
    ui_fill(x, y + 120, 584, 30, ui_rating_color(rating));
    ui_text(x + 10, y + 127, 2, C_BG, verdict[rating]);
    ui_textf(x, y + 160, 2, C_DIM, "Ruhe-Mitte: x %+.2f %%  y %+.2f %%", r->mean_x * 100, r->mean_y * 100);
}

static void draw_circle_view(const Input *in, int s, float cx, float cy, float r)
{
    ui_disc(cx, cy, r * 1.2f, C_BG);
    ui_circle(cx, cy, r, C_LINE);
    ui_line(cx - r, cy, cx + r, cy, C_PANEL3);
    ui_line(cx, cy - r, cx, cy + r, C_PANEL3);
    /* erreichte Segmente als Linienzug, Lücken bleiben offen */
    float lx = 0, ly = 0;
    bool prev = false;
    for (int i = 0; i <= CIRCLE_BINS; i++) {
        int b = i % CIRCLE_BINS;
        float rad = circle.radius[s][b];
        if (rad <= 0) {
            prev = false;
            continue;
        }
        float a = ((b + 0.5f) / CIRCLE_BINS) * 2 * SDL_PI_F - SDL_PI_F;
        float x = cx + cosf(a) * rad * r, y = cy + sinf(a) * rad * r;
        if (prev) {
            ui_line(lx, ly, x, y, C_ACCENT);
        }
        lx = x;
        ly = y;
        prev = true;
    }
    ui_disc(cx + in->x[s] * r, cy + in->y[s] * r, 5, C_TEXT);
}

static void draw_circle_controls(int s, float x, float button_y, float y)
{
    if (ui_button(x, button_y, 280, 30, circle.running[s] ? "Kreistest beenden" : "Kreistest starten", circle.running[s])) {
        if (!circle.running[s]) {
            memset(circle.radius[s], 0, sizeof circle.radius[s]);
            eventlog_add("Kreistest %s gestartet", s ? "rechts" : "links");
        }
        circle.running[s] = !circle.running[s];
    }
    CircleStats st = circle_stats(s);
    if (st.coverage > 0) {
        int rating = st.error < 8 ? 0 : st.error < 15 ? 1 : 2;
        ui_textf(x, y + 24, 2, st.coverage >= 99 ? C_GREEN : C_TEXT, "Abdeckung: %3.0f %%", st.coverage);
        ui_textf(x, y + 46, 2, C_TEXT, "Radius:    %3.0f %%", st.mean);
        ui_textf(x, y + 68, 2, st.min < 90 ? C_RED : C_TEXT, "Min.:      %3.0f %%", st.min);
        ui_textf(x, y + 90, 2, ui_rating_color(rating), "Fehler:  %5.1f %%", st.error);
    } else if (circle.running[s]) {
        ui_text(x, y + 24, 2, C_YELLOW, "Stick am Rand");
        ui_text(x, y + 46, 2, C_YELLOW, "kreisen lassen");
    }
}

static void draw_circle_verdict(int s, float x, float y)
{
    CircleStats st = circle_stats(s);
    if (circle.running[s] || st.coverage < 99) {
        return;
    }
    if (st.min < 90) {
        ui_text(x, y, 2, C_RED, "Rand wird nicht überall erreicht");
    } else if (st.error < 15) {
        ui_text(x, y, 2, C_GREEN, "Kreistest: Ausschlag in Ordnung");
    } else {
        ui_text(x, y, 2, C_YELLOW, "Kreistest: Ausschlag ungleichmäßig");
    }
}

static void draw_stick_panel(const Input *in, int s, float px)
{
    const char *names[2] = {"Linker Stick", "Rechter Stick"};
    const float py = 150;
    ui_fill(px, py, 616, 610, C_PANEL);
    ui_text(px + 16, py + 10, 2, C_ACCENT, names[s]);

    draw_zoom_view(in, s, px + 150, py + 160, 110);
    float live = hypotf(in->x[s], in->y[s]) * 100;
    ui_textf(px + 16, py + 300, 2, ui_rating_color(drift_rating(live)), "Live: %5.2f %%", live);
    draw_rest_result(s, px + 16, py + 330);

    draw_circle_view(in, s, px + 460, py + 160, 110);
    draw_circle_controls(s, px + 326, py + 300, py + 330);
    draw_circle_verdict(s, px + 16, py + 570);
}

static void draw_rest_status(Uint64 now)
{
    char buf[96];
    const char *status = "Ruhetest: beide Sticks 5 s lang nicht berühren.";
    SDL_Color color = C_DIM;
    switch (rest.state) {
    case REST_WAIT:
        snprintf(buf, sizeof buf, "Sticks LOSLASSEN! Messung beginnt in %.0f s...",
                 ceilf((REST_WAIT_MS - (float)(now - rest.t0)) / 1000));
        status = buf;
        color = C_YELLOW;
        break;
    case REST_RUN:
        snprintf(buf, sizeof buf, "Messe... %.1f s - nicht berühren!", (REST_RUN_MS - (float)(now - rest.t0)) / 1000);
        status = buf;
        color = C_YELLOW;
        break;
    case REST_ABORTED:
        status = "Abgebrochen: Stick wurde bewegt. Bitte erneut starten.";
        color = C_RED;
        break;
    case REST_DONE:
        status = "Ruhetest abgeschlossen.";
        color = C_GREEN;
        break;
    default:
        break;
    }
    ui_text(20, 116, 2, color, status);

    bool busy = rest.state == REST_WAIT || rest.state == REST_RUN;
    if (ui_button(950, 108, 310, 32, busy ? "Abbrechen" : "Ruhetest starten", busy)) {
        rest.state = busy ? REST_IDLE : REST_WAIT;
        rest.t0 = now;
    }
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    (void)g;
    draw_rest_status(SDL_GetTicks());
    draw_stick_panel(in, 0, 16);
    draw_stick_panel(in, 1, 648);
}

const Tab tab_drift = {"Stickdrift", reset, update, draw, reset};
