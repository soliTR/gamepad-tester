/*
 * "Gyro" tab: gyroscope and accelerometer
 *
 * The 3D view integrates the gyro's angular rate into an orientation.
 * The gyro rest test measures which angular rate the sensor reports although
 * the controller lies still (bias). That value can then be subtracted.
 */
#include <math.h>
#include <string.h>

#include "eventlog.h"
#include "quat.h"
#include "tabs.h"
#include "ui.h"

#define RAD2DEG (180.0f / SDL_PI_F)

#define GRAPH_LEN 400       /* points in the history graph */
#define REST_WAIT_MS 2000   /* time to put the controller down */
#define REST_RUN_MS 5000

static const SDL_Color C_AXIS[3] = {{232, 82, 82, 255}, {80, 200, 120, 255}, {90, 165, 255, 255}};

/* Latest readings */
static float gyro[3];  /* rad/s */
static float accel[3]; /* m/s^2 */
static bool have_gyro, have_accel;

/* Orientation for the 3D view */
static Quat orientation = {1, 0, 0, 0};
static Uint64 last_timestamp;

/* History in degrees/s */
static float graph[GRAPH_LEN][3];
static int graph_head, graph_n;

/* Bias correction in rad/s */
static float bias[3];
static bool bias_on;

typedef enum { REST_IDLE, REST_WAIT, REST_RUN, REST_DONE } RestState;

static struct {
    RestState state;
    Uint64 t0;
    double sum[3], sum_sq[3], accel_sum;
    int n, accel_n;
    /* Result */
    float bias[3], noise[3]; /* degrees/s */
    float gravity;           /* mean acceleration in g, should be ~1 */
} rest;

static void reset_view(void)
{
    orientation = QUAT_IDENTITY;
    last_timestamp = 0;
    graph_n = graph_head = 0;
    rest.state = REST_IDLE;
}

static void reset(void)
{
    reset_view();
    have_gyro = have_accel = false;
    bias_on = false;
    memset(bias, 0, sizeof bias);
}

/* ------------------------------------------------------------------ */
/* Measurement                                                         */
/* ------------------------------------------------------------------ */

static void on_gyro(const float *d, Uint64 ts)
{
    memcpy(gyro, d, sizeof gyro);
    have_gyro = true;

    float w[3];
    for (int i = 0; i < 3; i++) {
        w[i] = d[i] - (bias_on ? bias[i] : 0);
    }
    if (last_timestamp) {
        float dt = (float)(ts - last_timestamp) / 1e9f;
        if (dt > 0 && dt < 0.1f) { /* skip gaps (e.g. right after connecting) */
            orientation = quat_integrate(orientation, w, dt);
        }
    }
    last_timestamp = ts;

    for (int i = 0; i < 3; i++) {
        graph[graph_head][i] = w[i] * RAD2DEG;
    }
    graph_head = (graph_head + 1) % GRAPH_LEN;
    if (graph_n < GRAPH_LEN) {
        graph_n++;
    }

    if (rest.state == REST_RUN) {
        for (int i = 0; i < 3; i++) {
            double v = d[i] * RAD2DEG;
            rest.sum[i] += v;
            rest.sum_sq[i] += v * v;
        }
        rest.n++;
    }
}

static void on_accel(const float *d)
{
    memcpy(accel, d, sizeof accel);
    have_accel = true;
    if (rest.state == REST_RUN) {
        rest.accel_sum += sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / SDL_STANDARD_GRAVITY;
        rest.accel_n++;
    }
}

void gyro_on_sensor(SDL_SensorType type, const float *data, Uint64 timestamp_ns)
{
    if (timestamp_ns == 0) {
        timestamp_ns = SDL_GetTicksNS();
    }
    if (type == SDL_SENSOR_GYRO) {
        on_gyro(data, timestamp_ns);
    } else if (type == SDL_SENSOR_ACCEL) {
        on_accel(data);
    }
}

static void finish_rest_test(void)
{
    for (int i = 0; i < 3; i++) {
        double mean = rest.sum[i] / rest.n;
        rest.bias[i] = (float)mean;
        rest.noise[i] = (float)sqrt(SDL_max(0.0, rest.sum_sq[i] / rest.n - mean * mean));
        bias[i] = (float)mean / RAD2DEG;
    }
    rest.gravity = rest.accel_n > 0 ? (float)(rest.accel_sum / rest.accel_n) : 0;
    bias_on = true;
    rest.state = REST_DONE;
    eventlog_add("Gyro rest test done");
}

static void update(SDL_Gamepad *g, const Input *in)
{
    (void)g;
    (void)in;
    Uint64 now = SDL_GetTicks();
    if (rest.state == REST_WAIT && now - rest.t0 >= REST_WAIT_MS) {
        Uint64 t0 = now;
        memset(&rest, 0, sizeof rest);
        rest.state = REST_RUN;
        rest.t0 = t0;
    } else if (rest.state == REST_RUN && now - rest.t0 >= REST_RUN_MS) {
        if (rest.n > 0) {
            finish_rest_test();
        } else {
            rest.state = REST_IDLE;
            eventlog_add("Gyro rest test: no sensor data");
        }
    }
}

/* ------------------------------------------------------------------ */
/* 3D view                                                             */
/* ------------------------------------------------------------------ */

static const float VIEW_CX = 326, VIEW_CY = 314;

/* Rotates a point in controller coordinates and projects it onto the screen in perspective */
static void project(const float p[3], float out[2])
{
    float r[3];
    quat_rotate(orientation, p, r);
    const float a = 0.45f; /* camera looks slightly from above */
    float y = r[1] * cosf(a) - r[2] * sinf(a);
    float z = r[1] * sinf(a) + r[2] * cosf(a);
    float d = 4.0f - z;
    out[0] = VIEW_CX + 620 * r[0] / d;
    out[1] = VIEW_CY - 620 * y / d;
}

/* Wireframe box with center c and half extents h */
static void draw_box(float cx, float cy, float cz, float hx, float hy, float hz, SDL_Color color)
{
    float v[8][2];
    for (int i = 0; i < 8; i++) {
        float p[3] = {cx + ((i & 1) ? hx : -hx), cy + ((i & 2) ? hy : -hy), cz + ((i & 4) ? hz : -hz)};
        project(p, v[i]);
    }
    /* Edges connect corners that differ in exactly one coordinate */
    for (int i = 0; i < 8; i++) {
        for (int bit = 1; bit < 8; bit <<= 1) {
            if (!(i & bit)) {
                ui_line(v[i][0], v[i][1], v[i | bit][0], v[i | bit][1], color);
            }
        }
    }
}

static void draw_segment(const float a[3], const float b[3], SDL_Color color)
{
    float pa[2], pb[2];
    project(a, pa);
    project(b, pb);
    ui_line(pa[0], pa[1], pb[0], pb[1], color);
}

static void draw_controller_3d(void)
{
    /* SDL coordinates: +X right, +Y up, -Z away from the player */
    draw_box(0, 0, 0, 1.0f, 0.15f, 0.45f, C_TEXT);                  /* body */
    draw_box(-0.72f, -0.02f, 0.62f, 0.26f, 0.13f, 0.2f, C_DIM);     /* grips */
    draw_box(0.72f, -0.02f, 0.62f, 0.26f, 0.13f, 0.2f, C_DIM);
    draw_box(-0.65f, 0.05f, -0.52f, 0.22f, 0.08f, 0.08f, C_ACCENT); /* shoulder buttons */
    draw_box(0.65f, 0.05f, -0.52f, 0.22f, 0.08f, 0.08f, C_ACCENT);
    const float stick_l[2][3] = {{-0.45f, 0.15f, 0.05f}, {-0.45f, 0.4f, 0.05f}};
    const float stick_r[2][3] = {{0.45f, 0.15f, 0.05f}, {0.45f, 0.4f, 0.05f}};
    draw_segment(stick_l[0], stick_l[1], C_PRESS);
    draw_segment(stick_r[0], stick_r[1], C_PRESS);
    const float front[2][3] = {{0, 0.15f, -0.45f}, {0, 0.15f, -0.95f}};
    draw_segment(front[0], front[1], C_GREEN);
    float label[2];
    project(front[1], label);
    ui_text(label[0] + 6, label[1] - 8, 2, C_GREEN, "front");
}

/* ------------------------------------------------------------------ */
/* Display                                                             */
/* ------------------------------------------------------------------ */

static void draw_graph(float x, float y, float w, float h)
{
    ui_fill(x, y, w, h, C_PANEL);
    /* The scale adapts to the largest value, at least +/-5 °/s */
    float range = 5;
    for (int i = 0; i < graph_n; i++) {
        for (int a = 0; a < 3; a++) {
            range = SDL_max(range, fabsf(graph[i][a]));
        }
    }
    ui_line(x, y + h / 2, x + w, y + h / 2, C_PANEL3);
    for (int a = 0; a < 3; a++) {
        float px = 0, py = 0;
        for (int i = 0; i < graph_n; i++) {
            int idx = (graph_head - graph_n + i + GRAPH_LEN) % GRAPH_LEN;
            float gx = x + w * i / (float)(GRAPH_LEN - 1);
            float gy = y + h / 2 - graph[idx][a] / range * (h / 2 - 4);
            if (i > 0) {
                ui_line(px, py, gx, gy, C_AXIS[a]);
            }
            px = gx;
            py = gy;
        }
    }
    ui_textf(x + 8, y + 6, 2, C_DIM, "Gyro history  +/-%.0f °/s", range);
}

/* Bar that grows from the center to the left (negative) or right (positive) */
static void draw_center_bar(float x, float y, float w, float h, float v, float range, SDL_Color c)
{
    ui_fill(x, y, w, h, C_PANEL);
    float f = SDL_clamp(v / range, -1.0f, 1.0f);
    float mid = x + w / 2;
    if (f >= 0) {
        ui_fill(mid, y, w / 2 * f, h, c);
    } else {
        ui_fill(mid + w / 2 * f, y, -w / 2 * f, h, c);
    }
    ui_line(mid, y, mid, y + h, C_LINE);
}

static float draw_readings(SDL_Gamepad *g, bool has_gyro, bool has_accel, float x, float y)
{
    static const char *axis_names[3] = {"X (pitch)", "Y (yaw)", "Z (roll)"};
    ui_text(x, y, 2, C_ACCENT, "Gyro (°/s)");
    for (int a = 0; a < 3; a++) {
        float v = (gyro[a] - (bias_on ? bias[a] : 0)) * RAD2DEG;
        y += 24;
        ui_text(x, y, 2, C_AXIS[a], axis_names[a]);
        ui_textf(x + 180, y, 2, C_TEXT, "%+8.2f", has_gyro && have_gyro ? v : 0.0f);
        draw_center_bar(x + 340, y, 250, 16, v, 360, C_AXIS[a]);
    }
    y += 40;
    ui_text(x, y, 2, C_ACCENT, "Acceleration (m/s^2)");
    for (int a = 0; a < 3; a++) {
        y += 24;
        ui_textf(x, y, 2, C_AXIS[a], "%c", "XYZ"[a]);
        ui_textf(x + 180, y, 2, C_TEXT, "%+8.2f", has_accel && have_accel ? accel[a] : 0.0f);
        draw_center_bar(x + 340, y, 250, 16, accel[a], 20, C_AXIS[a]);
    }
    y += 24;
    float g_total = sqrtf(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]) / SDL_STANDARD_GRAVITY;
    ui_textf(x, y, 2, C_TEXT, "Total: %.2f g", g_total);
    y += 24;
    ui_textf(x, y, 2, C_DIM, "Data rate: gyro %.0f Hz, accel %.0f Hz",
             has_gyro ? SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO) : 0.0f,
             has_accel ? SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_ACCEL) : 0.0f);
    if (has_gyro && !have_gyro) {
        y += 24;
        ui_text(x, y, 2, C_YELLOW, "No gyro data received yet.");
    }
    return y + 36;
}

static float draw_controls(bool has_gyro, float x, float y, Uint64 now)
{
    if (ui_button(x, y, 300, 34, "Reset orientation", false)) {
        reset_view();
    }
    bool busy = rest.state == REST_WAIT || rest.state == REST_RUN;
    if (has_gyro && ui_button(x + 310, y, 300, 34, busy ? "Cancel" : "Gyro rest test", busy)) {
        rest.state = busy ? REST_IDLE : REST_WAIT;
        rest.t0 = now;
    }
    y += 44;
    if (has_gyro && ui_button(x, y, 300, 34, bias_on ? "Bias correction on" : "Bias correction off", bias_on)) {
        bias_on = !bias_on;
        last_timestamp = 0;
    }
    return y + 50;
}

static void draw_rest_test(bool has_accel, float x, float y, Uint64 now)
{
    switch (rest.state) {
    case REST_WAIT:
        ui_textf(x, y, 2, C_YELLOW, "Put it down flat, don't touch... %.0f",
                 ceilf((REST_WAIT_MS - (float)(now - rest.t0)) / 1000));
        break;
    case REST_RUN:
        ui_textf(x, y, 2, C_YELLOW, "Measuring... %.1f s", (REST_RUN_MS - (float)(now - rest.t0)) / 1000);
        break;
    case REST_DONE: {
        static const char *verdict[3] = {"Gyro OK - very little drift", "Gyro slightly inaccurate", "Gyro drifts noticeably"};
        float worst = SDL_max(fabsf(rest.bias[0]), SDL_max(fabsf(rest.bias[1]), fabsf(rest.bias[2])));
        int rating = worst < 1.0f ? 0 : worst < 3.0f ? 1 : 2;
        ui_text(x, y, 2, C_ACCENT, "Gyro rest test");
        ui_textf(x, y + 24, 2, ui_rating_color(rating), "Bias:     %+5.2f %+5.2f %+5.2f °/s", rest.bias[0], rest.bias[1],
                 rest.bias[2]);
        ui_textf(x, y + 46, 2, C_TEXT, "Noise:     %5.2f  %5.2f  %5.2f °/s", rest.noise[0], rest.noise[1], rest.noise[2]);
        if (has_accel) {
            bool ok = rest.gravity > 0.9f && rest.gravity < 1.1f;
            ui_textf(x, y + 68, 2, ok ? C_GREEN : C_YELLOW, "Gravity: %.3f g (expected ~1)", rest.gravity);
        }
        ui_fill(x, y + 96, 600, 30, ui_rating_color(rating));
        ui_text(x + 10, y + 103, 2, C_BG, verdict[rating]);
        ui_text(x, y + 136, 2, C_DIM, "Bias is now subtracted automatically.");
        break;
    }
    default:
        ui_text(x, y, 2, C_DIM, "Gyro rest test: press the button,");
        ui_text(x, y + 22, 2, C_DIM, "put the controller down, wait 7 s.");
        break;
    }
}

static void draw(SDL_Gamepad *g, const Input *in)
{
    (void)in;
    bool has_gyro = SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO);
    bool has_accel = SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL);
    if (!has_gyro && !has_accel) {
        ui_text_center(640, 380, 2, C_YELLOW, "This controller reports no gyroscope or accelerometer.");
        ui_text_center(640, 410, 2, C_DIM, "8BitDo controllers often have a gyro only in Switch mode.");
        return;
    }
    Uint64 now = SDL_GetTicks();

    ui_fill(16, 104, 620, 420, C_PANEL);
    if (has_gyro) {
        draw_controller_3d();
    }
    ui_text(26, 112, 2, C_DIM, "3D view from the gyro");
    ui_text(26, 496, 1, C_DIM, "Hold the controller normally and press [R] to reset the orientation.");
    draw_graph(16, 534, 620, 226);

    float x = 652;
    float y = draw_readings(g, has_gyro, has_accel, x, 112);
    y = draw_controls(has_gyro, x, y, now);
    draw_rest_test(has_accel, x, y, now);
}

const Tab tab_gyro = {"Gyro", reset, update, draw, reset_view};
