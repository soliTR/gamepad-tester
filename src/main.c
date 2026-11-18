/*
 * Gamepad-Tester: Tasten, Sticks, Stickdrift, Vibration, Gyro, Touchpad und LED
 * eines Gamepads testen. Benötigt nur SDL3 (>= 3.4).
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define W 1280
#define H 800
#define MAX_PADS 8
#define PI_F 3.14159265f
#define RAD2DEG (180.0f / PI_F)

#define TRAIL_LEN 90
#define RT_MAX 4096     /* Samples im Stick-Ruhetest */
#define RT_WAIT_MS 3000 /* Countdown vor der Messung */
#define RT_RUN_MS 5000  /* Messdauer */
#define CT_BINS 72      /* Winkelsegmente im Kreistest (je 5 Grad) */
#define GG_LEN 400      /* Punkte im Gyro-Graph */
#define GT_WAIT_MS 2000
#define GT_RUN_MS 5000

/* Grenzwerte für die Drift-Ampel (maximale Auslenkung in Ruhe, Prozent) */
#define DRIFT_OK 3.0f
#define DRIFT_WARN 7.0f

static SDL_Window *win;
static SDL_Renderer *ren;

static const SDL_Color C_BG = {18, 20, 26, 255};
static const SDL_Color C_PANEL = {28, 32, 42, 255};
static const SDL_Color C_PANEL2 = {42, 48, 62, 255};
static const SDL_Color C_PANEL3 = {58, 66, 86, 255};
static const SDL_Color C_LINE = {78, 88, 110, 255};
static const SDL_Color C_TEXT = {224, 228, 236, 255};
static const SDL_Color C_DIM = {132, 140, 156, 255};
static const SDL_Color C_ACCENT = {90, 165, 255, 255};
static const SDL_Color C_GREEN = {80, 200, 120, 255};
static const SDL_Color C_GREEN_DIM = {40, 100, 64, 255};
static const SDL_Color C_YELLOW = {232, 192, 60, 255};
static const SDL_Color C_RED = {232, 82, 82, 255};
static const SDL_Color C_PRESS = {255, 168, 40, 255};
static const SDL_Color C_AX[3] = {{232, 82, 82, 255}, {80, 200, 120, 255}, {90, 165, 255, 255}};

/* ------------------------------------------------------------------ */
/* Zeichnen                                                            */
/* ------------------------------------------------------------------ */

static void set_color(SDL_Color c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); }

static void fill(float x, float y, float w, float h, SDL_Color c)
{
    SDL_FRect r = {x, y, w, h};
    set_color(c);
    SDL_RenderFillRect(ren, &r);
}

static void outline(float x, float y, float w, float h, SDL_Color c)
{
    SDL_FRect r = {x, y, w, h};
    set_color(c);
    SDL_RenderRect(ren, &r);
}

static void line(float x1, float y1, float x2, float y2, SDL_Color c)
{
    set_color(c);
    SDL_RenderLine(ren, x1, y1, x2, y2);
}

static void circle(float cx, float cy, float r, SDL_Color c)
{
    set_color(c);
    float px = cx + r, py = cy;
    for (int i = 1; i <= 64; i++) {
        float a = i * 2 * PI_F / 64;
        float nx = cx + r * cosf(a), ny = cy + r * sinf(a);
        SDL_RenderLine(ren, px, py, nx, ny);
        px = nx;
        py = ny;
    }
}

static void disc(float cx, float cy, float r, SDL_Color c)
{
    set_color(c);
    for (float dy = -r; dy <= r; dy += 1) {
        float hw = sqrtf(r * r - dy * dy);
        SDL_RenderLine(ren, cx - hw, cy + dy, cx + hw, cy + dy);
    }
}

/*
 * Text über die eingebaute 8x8-Schrift von SDL. Die kennt nur ASCII,
 * deshalb werden Umlaute als Grundbuchstabe plus zwei Punkte gezeichnet.
 */
static float text(float x, float y, float s, SDL_Color c, const char *str)
{
    set_color(c);
    SDL_SetRenderScale(ren, s, s);
    float cx = x / s, cy = y / s;
    const char *p = str;
    while (*p) {
        Uint32 cp = SDL_StepUTF8(&p, NULL);
        char ch[2] = {0, 0};
        const char *rep = NULL;
        int dots = 0; /* 1 = Kleinbuchstabe, 2 = Großbuchstabe */
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
        case 0xB0: { /* Gradzeichen */
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

static float text_width(float s, const char *str)
{
    int n = 0;
    const char *p = str;
    while (*p) {
        Uint32 cp = SDL_StepUTF8(&p, NULL);
        n += cp == 0xDF ? 2 : cp == 0x2026 ? 3 : cp == 0x2192 ? 2 : 1;
    }
    return n * 8 * s;
}

static float textf(float x, float y, float s, SDL_Color c, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return text(x, y, s, c, buf);
}

static void text_center(float cx, float y, float s, SDL_Color c, const char *str)
{
    text(cx - text_width(s, str) / 2, y, s, c, str);
}

static SDL_Color rating_color(int r) { return r == 0 ? C_GREEN : r == 1 ? C_YELLOW : C_RED; }

/* ------------------------------------------------------------------ */
/* Maus und Schaltflächen                                              */
/* ------------------------------------------------------------------ */

static float mx, my;
static bool mdown, mclick;

static bool hover(float x, float y, float w, float h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static bool ui_button(float x, float y, float w, float h, const char *label, bool on)
{
    bool hv = hover(x, y, w, h);
    fill(x, y, w, h, on ? C_ACCENT : hv ? C_PANEL3 : C_PANEL2);
    outline(x, y, w, h, C_LINE);
    text(x + (w - text_width(2, label)) / 2, y + (h - 16) / 2, 2, on ? C_BG : C_TEXT, label);
    return hv && mclick;
}

/* Schieberegler 0..1, verändert *v solange die Maus darauf gedrückt ist */
static void ui_slider(float x, float y, float w, float *v)
{
    if (mdown && hover(x - 8, y - 8, w + 16, 36)) {
        *v = SDL_clamp((mx - x) / w, 0.0f, 1.0f);
    }
    fill(x, y + 6, w, 8, C_PANEL2);
    fill(x, y + 6, w * *v, 8, C_ACCENT);
    disc(x + w * *v, y + 10, 10, C_TEXT);
    textf(x + w + 20, y + 2, 2, C_TEXT, "%3.0f %%", *v * 100);
}

/* ------------------------------------------------------------------ */
/* Ereignis-Log                                                        */
/* ------------------------------------------------------------------ */

#define LOG_LINES 24
static char logbuf[LOG_LINES][64];
static int lognext, logcount;

static void logmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(logbuf[lognext], sizeof logbuf[0], fmt, ap);
    va_end(ap);
    lognext = (lognext + 1) % LOG_LINES;
    if (logcount < LOG_LINES) {
        logcount++;
    }
}

/* Kürzt str auf höchstens max Zeichen (UTF-8-sicher) */
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

static void draw_log(float x, float y, int lines, int maxchars)
{
    char buf[64];
    for (int i = 0; i < lines && i < logcount; i++) {
        int idx = (lognext - 1 - i + LOG_LINES) % LOG_LINES;
        clip(buf, sizeof buf, logbuf[idx], maxchars);
        text(x, y + i * 20, 2, i == 0 ? C_TEXT : C_DIM, buf);
    }
}

/* ------------------------------------------------------------------ */
/* Controller-Verwaltung und Namen                                     */
/* ------------------------------------------------------------------ */

static SDL_Gamepad *pads[MAX_PADS];
static int npads;
static int act = -1;

static SDL_Gamepad *cur(void) { return act >= 0 ? pads[act] : NULL; }

static bool is_ps(SDL_GamepadType t)
{
    return t == SDL_GAMEPAD_TYPE_PS3 || t == SDL_GAMEPAD_TYPE_PS4 || t == SDL_GAMEPAD_TYPE_PS5;
}

static bool is_nin(SDL_GamepadType t)
{
    return t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
           t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR ||
           t == SDL_GAMEPAD_TYPE_GAMECUBE;
}

static const char *type_name(SDL_GamepadType t)
{
    switch (t) {
    case SDL_GAMEPAD_TYPE_STANDARD: return "Standard-Gamepad";
    case SDL_GAMEPAD_TYPE_XBOX360: return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE: return "Xbox One / Series";
    case SDL_GAMEPAD_TYPE_PS3: return "PlayStation 3";
    case SDL_GAMEPAD_TYPE_PS4: return "PlayStation 4 (DualShock 4)";
    case SDL_GAMEPAD_TYPE_PS5: return "PlayStation 5 (DualSense)";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return "Nintendo Switch Pro";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "Joy-Con links";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "Joy-Con rechts";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Joy-Con Paar";
    case SDL_GAMEPAD_TYPE_GAMECUBE: return "GameCube";
    case SDL_GAMEPAD_TYPE_STEAM: return "Steam Controller";
    default: return "Unbekannt";
    }
}

static const char *btn_name(SDL_Gamepad *g, SDL_GamepadButton b)
{
    static const char *misc_names[] = {"Zusatz 2", "Zusatz 3", "Zusatz 4", "Zusatz 5", "Zusatz 6"};
    SDL_GamepadType t = SDL_GetGamepadType(g);
    bool ps = is_ps(t), nin = is_nin(t);
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH:
        switch (SDL_GetGamepadButtonLabel(g, b)) {
        case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "Kreuz";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "Kreis";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "Quadrat";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Dreieck";
        default: break;
        }
        return b == SDL_GAMEPAD_BUTTON_SOUTH ? "Taste unten" : b == SDL_GAMEPAD_BUTTON_EAST ? "Taste rechts"
             : b == SDL_GAMEPAD_BUTTON_WEST  ? "Taste links" : "Taste oben";
    case SDL_GAMEPAD_BUTTON_BACK: return ps ? (t == SDL_GAMEPAD_TYPE_PS5 ? "Create" : "Share") : nin ? "Minus (-)" : "Select";
    case SDL_GAMEPAD_BUTTON_GUIDE: return ps ? "PS-Taste" : "Home";
    case SDL_GAMEPAD_BUTTON_START: return ps ? "Options" : nin ? "Plus (+)" : "Start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return ps ? "L3 (Stick-Klick)" : "L-Stick-Klick";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return ps ? "R3 (Stick-Klick)" : "R-Stick-Klick";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ps ? "L1" : nin ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ps ? "R1" : nin ? "R" : "RB";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-Pad hoch";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-Pad runter";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-Pad links";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-Pad rechts";
    case SDL_GAMEPAD_BUTTON_MISC1: return ps ? "Mikrofon" : nin ? "Capture" : "Zusatz 1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "Paddle R1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "Paddle L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "Paddle R2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "Paddle L2";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad-Klick";
    case SDL_GAMEPAD_BUTTON_MISC2:
    case SDL_GAMEPAD_BUTTON_MISC3:
    case SDL_GAMEPAD_BUTTON_MISC4:
    case SDL_GAMEPAD_BUTTON_MISC5:
    case SDL_GAMEPAD_BUTTON_MISC6: return misc_names[b - SDL_GAMEPAD_BUTTON_MISC2];
    default: return "?";
    }
}

static const char *trig_name(SDL_Gamepad *g, int right)
{
    SDL_GamepadType t = SDL_GetGamepadType(g);
    if (is_ps(t)) {
        return right ? "R2" : "L2";
    }
    if (is_nin(t)) {
        return right ? "ZR" : "ZL";
    }
    return right ? "RT" : "LT";
}

static bool cap(SDL_Gamepad *g, const char *prop)
{
    return SDL_GetBooleanProperty(SDL_GetGamepadProperties(g), prop, false);
}

/* ------------------------------------------------------------------ */
/* Quaternionen für die 3D-Ansicht                                     */
/* ------------------------------------------------------------------ */

typedef struct { float w, x, y, z; } Quat;

static Quat qmul(Quat a, Quat b)
{
    return (Quat){a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                  a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                  a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                  a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

static void qrot(Quat q, const float v[3], float out[3])
{
    Quat p = {0, v[0], v[1], v[2]};
    Quat c = {q.w, -q.x, -q.y, -q.z};
    Quat r = qmul(qmul(q, p), c);
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

/* Dreht q um die Winkelgeschwindigkeit w (rad/s, Controller-Koordinaten) für dt Sekunden */
static Quat qintegrate(Quat q, const float w[3], float dt)
{
    float n = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    if (n < 1e-9f) {
        return q;
    }
    float half = n * dt / 2, s = sinf(half) / n;
    Quat d = {cosf(half), w[0] * s, w[1] * s, w[2] * s};
    Quat r = qmul(q, d);
    float len = sqrtf(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
    return (Quat){r.w / len, r.x / len, r.y / len, r.z / len};
}

/* ------------------------------------------------------------------ */
/* Teststatus (wird beim Controllerwechsel zurückgesetzt)              */
/* ------------------------------------------------------------------ */

enum { TAB_INFO, TAB_BUTTONS, TAB_STICKS, TAB_DRIFT, TAB_RUMBLE, TAB_GYRO, TAB_EXTRAS, TAB_COUNT };
static const char *tab_names[TAB_COUNT] = {"Übersicht", "Tasten", "Sticks", "Stickdrift", "Vibration", "Gyro", "Extras"};
static int tab = TAB_INFO;

/* aktuelle Stickwerte, -1..1, Y positiv = unten (wie SDL) */
static float st_x[2], st_y[2], trig[2];

static bool seen[SDL_GAMEPAD_BUTTON_COUNT];
static bool seen_trig[2];

typedef struct {
    float x[TRAIL_LEN], y[TRAIL_LEN];
    int head, n;
} Trail;
static Trail trail[2];

enum { RT_IDLE, RT_WAIT, RT_RUN, RT_DONE, RT_ABORT };
static int rt_state;
static Uint64 rt_t0;
static int rt_n;
static float rt_s[RT_MAX][2][2];
static float rt_off[2], rt_noise[2], rt_max[2], rt_mx[2], rt_my[2];

static bool ct_run[2];
static float ct_bin[2][CT_BINS];

static Quat orient = {1, 0, 0, 0};
static Uint64 gyro_last_ts;
static float gyro[3], accel[3]; /* rad/s und m/s^2 */
static bool have_gyro, have_accel;
static float gbias[3];          /* rad/s */
static bool gbias_on;
static float ggraph[GG_LEN][3]; /* Grad/s */
static int gg_head, gg_n;

enum { GT_IDLE, GT_WAIT, GT_RUN, GT_DONE };
static int gt_state;
static Uint64 gt_t0;
static double gt_sum[3], gt_sq[3], gt_asum;
static int gt_n, gt_an;
static float gt_bias[3], gt_noise[3], gt_amag;

enum { RM_NONE, RM_LOW, RM_HIGH, RM_BOTH, RM_CONT, RM_ALT, RM_RAMP, RM_TRIG_L, RM_TRIG_R, RM_TRIG_B };
static int rm_mode;
static Uint64 rm_t0;
static float rm_int = 0.7f;
static Uint16 rm_cur[4]; /* tief, hoch, Trigger links, Trigger rechts */
static Uint64 rm_last_send;
static char rm_err[160];

static bool led_rainbow;
static Uint64 led_last;
static SDL_Color led_color = {0, 0, 255, 255};

typedef struct {
    float x[TRAIL_LEN], y[TRAIL_LEN];
    int head, n;
} TouchTrail;
static TouchTrail touch_trail[2];

static void reset_buttons(void)
{
    memset(seen, 0, sizeof seen);
    memset(seen_trig, 0, sizeof seen_trig);
}

static void reset_gyro(void)
{
    orient = (Quat){1, 0, 0, 0};
    gyro_last_ts = 0;
    gg_n = gg_head = 0;
    gt_state = GT_IDLE;
}

static void reset_tests(void)
{
    reset_buttons();
    memset(trail, 0, sizeof trail);
    memset(touch_trail, 0, sizeof touch_trail);
    rt_state = RT_IDLE;
    memset(ct_run, 0, sizeof ct_run);
    memset(ct_bin, 0, sizeof ct_bin);
    reset_gyro();
    have_gyro = have_accel = false;
    gbias_on = false;
    memset(gbias, 0, sizeof gbias);
    rm_mode = RM_NONE;
    memset(rm_cur, 0, sizeof rm_cur);
    rm_err[0] = 0;
    led_rainbow = false;
}

static void stop_rumble(SDL_Gamepad *g)
{
    if (g) {
        SDL_RumbleGamepad(g, 0, 0, 0);
        SDL_RumbleGamepadTriggers(g, 0, 0, 0);
    }
}

static void select_pad(int i)
{
    stop_rumble(cur());
    act = i;
    reset_tests();
}

static void add_pad(SDL_JoystickID id)
{
    if (npads >= MAX_PADS) {
        return;
    }
    SDL_Gamepad *g = SDL_OpenGamepad(id);
    if (!g) {
        logmsg("Fehler: %s", SDL_GetError());
        return;
    }
    if (SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO)) {
        SDL_SetGamepadSensorEnabled(g, SDL_SENSOR_GYRO, true);
    }
    if (SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL)) {
        SDL_SetGamepadSensorEnabled(g, SDL_SENSOR_ACCEL, true);
    }
    pads[npads++] = g;
    logmsg("Verbunden: %.40s", SDL_GetGamepadName(g));
    SDL_Log("Verbunden: %s | Typ %s | %04x:%04x | Verbindung %d | Rumble %d, Trigger-Rumble %d, RGB %d, "
            "Player-LED %d | Gyro %d (%.0f Hz), Accel %d | Touchpads %d",
            SDL_GetGamepadName(g), type_name(SDL_GetGamepadType(g)), SDL_GetGamepadVendor(g), SDL_GetGamepadProduct(g),
            (int)SDL_GetGamepadConnectionState(g), cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN),
            cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN), cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN),
            cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN), SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO),
            SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO), SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL),
            SDL_GetNumGamepadTouchpads(g));
    select_pad(npads - 1);
}

static void remove_pad(SDL_JoystickID id)
{
    for (int i = 0; i < npads; i++) {
        if (SDL_GetGamepadID(pads[i]) != id) {
            continue;
        }
        logmsg("Getrennt: %.42s", SDL_GetGamepadName(pads[i]));
        SDL_CloseGamepad(pads[i]);
        memmove(&pads[i], &pads[i + 1], (size_t)(npads - i - 1) * sizeof pads[0]);
        npads--;
        if (i == act) {
            act = -1;
            select_pad(npads > 0 ? 0 : -1);
        } else if (i < act) {
            act--;
        }
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Messungen pro Frame                                                 */
/* ------------------------------------------------------------------ */

static float nax(Sint16 v) { return v < 0 ? v / 32768.0f : v / 32767.0f; }

static int drift_rating(float v) { return v <= DRIFT_OK ? 0 : v <= DRIFT_WARN ? 1 : 2; }

static void finish_rest_test(void)
{
    for (int s = 0; s < 2; s++) {
        double sx = 0, sy = 0;
        for (int i = 0; i < rt_n; i++) {
            sx += rt_s[i][s][0];
            sy += rt_s[i][s][1];
        }
        rt_mx[s] = (float)(sx / rt_n);
        rt_my[s] = (float)(sy / rt_n);
        float noise = 0, mx_ = 0;
        for (int i = 0; i < rt_n; i++) {
            float x = rt_s[i][s][0], y = rt_s[i][s][1];
            noise = SDL_max(noise, hypotf(x - rt_mx[s], y - rt_my[s]));
            mx_ = SDL_max(mx_, hypotf(x, y));
        }
        rt_off[s] = hypotf(rt_mx[s], rt_my[s]) * 100;
        rt_noise[s] = noise * 100;
        rt_max[s] = mx_ * 100;
    }
    rt_state = RT_DONE;
    logmsg("Ruhetest fertig (%d Messwerte)", rt_n);
}

static void finish_gyro_test(void)
{
    for (int i = 0; i < 3; i++) {
        double m = gt_sum[i] / gt_n;
        gt_bias[i] = (float)m;
        gt_noise[i] = (float)sqrt(SDL_max(0.0, gt_sq[i] / gt_n - m * m));
        gbias[i] = (float)m / RAD2DEG;
    }
    gt_amag = gt_an > 0 ? (float)(gt_asum / gt_an) : 0;
    gbias_on = true;
    gt_state = GT_DONE;
    logmsg("Gyro-Ruhetest fertig");
}

static void set_rumble(SDL_Gamepad *g, Uint64 now)
{
    float t = (float)(now - rm_t0) / 1000.0f;
    Uint16 full = (Uint16)(rm_int * 65535);
    Uint16 v[4] = {0, 0, 0, 0};
    switch (rm_mode) {
    case RM_LOW: v[0] = full; if (t > 1.5f) rm_mode = RM_NONE; break;
    case RM_HIGH: v[1] = full; if (t > 1.5f) rm_mode = RM_NONE; break;
    case RM_BOTH: v[0] = v[1] = full; if (t > 1.5f) rm_mode = RM_NONE; break;
    case RM_CONT: v[0] = v[1] = full; break;
    case RM_ALT:
        v[((int)(t / 0.4f)) % 2] = full;
        if (t > 4.0f) rm_mode = RM_NONE;
        break;
    case RM_RAMP:
        v[0] = v[1] = (Uint16)(SDL_min(t / 3.0f, 1.0f) * full);
        if (t > 3.5f) rm_mode = RM_NONE;
        break;
    case RM_TRIG_L: v[2] = full; if (t > 1.5f) rm_mode = RM_NONE; break;
    case RM_TRIG_R: v[3] = full; if (t > 1.5f) rm_mode = RM_NONE; break;
    case RM_TRIG_B: v[2] = v[3] = full; if (t > 1.5f) rm_mode = RM_NONE; break;
    default: break;
    }
    if (rm_mode == RM_NONE) {
        memset(v, 0, sizeof v);
    }
    bool changed = memcmp(v, rm_cur, sizeof v) != 0;
    bool active = v[0] || v[1] || v[2] || v[3];
    if (!changed && !(active && now - rm_last_send > 1000)) {
        return;
    }
    rm_last_send = now;
    if ((v[0] != rm_cur[0] || v[1] != rm_cur[1] || active) && (v[0] || v[1] || rm_cur[0] || rm_cur[1])) {
        if (!SDL_RumbleGamepad(g, v[0], v[1], 2000)) {
            snprintf(rm_err, sizeof rm_err, "Vibration fehlgeschlagen: %s", SDL_GetError());
        }
    }
    if ((v[2] || v[3] || rm_cur[2] || rm_cur[3])) {
        if (!SDL_RumbleGamepadTriggers(g, v[2], v[3], 2000)) {
            snprintf(rm_err, sizeof rm_err, "Trigger-Vibration fehlgeschlagen: %s", SDL_GetError());
        }
    }
    memcpy(rm_cur, v, sizeof v);
}

static SDL_Color hue(float h)
{
    float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
    return (SDL_Color){(Uint8)(SDL_clamp(r, 0.0f, 1.0f) * 255), (Uint8)(SDL_clamp(g, 0.0f, 1.0f) * 255),
                       (Uint8)(SDL_clamp(b, 0.0f, 1.0f) * 255), 255};
}

static void update(void)
{
    SDL_Gamepad *g = cur();
    if (!g) {
        return;
    }
    Uint64 now = SDL_GetTicks();
    st_x[0] = nax(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX));
    st_y[0] = nax(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY));
    st_x[1] = nax(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX));
    st_y[1] = nax(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY));
    trig[0] = SDL_max(0.0f, nax(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)));
    trig[1] = SDL_max(0.0f, nax(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)));

    for (int s = 0; s < 2; s++) {
        Trail *t = &trail[s];
        t->x[t->head] = st_x[s];
        t->y[t->head] = st_y[s];
        t->head = (t->head + 1) % TRAIL_LEN;
        if (t->n < TRAIL_LEN) {
            t->n++;
        }
        if (trig[s] >= 0.95f && !seen_trig[s]) {
            seen_trig[s] = true;
            logmsg("%s voll durchgedrückt", trig_name(g, s));
        }
        if (ct_run[s]) {
            float r = hypotf(st_x[s], st_y[s]);
            if (r > 0.5f) {
                int bin = (int)((atan2f(st_y[s], st_x[s]) + PI_F) / (2 * PI_F) * CT_BINS) % CT_BINS;
                ct_bin[s][bin] = SDL_max(ct_bin[s][bin], r);
            }
        }
    }

    if (rt_state == RT_WAIT && now - rt_t0 >= RT_WAIT_MS) {
        rt_state = RT_RUN;
        rt_t0 = now;
        rt_n = 0;
    } else if (rt_state == RT_RUN) {
        if (hypotf(st_x[0], st_y[0]) > 0.3f || hypotf(st_x[1], st_y[1]) > 0.3f) {
            rt_state = RT_ABORT;
        } else {
            if (rt_n < RT_MAX) {
                for (int s = 0; s < 2; s++) {
                    rt_s[rt_n][s][0] = st_x[s];
                    rt_s[rt_n][s][1] = st_y[s];
                }
                rt_n++;
            }
            if (now - rt_t0 >= RT_RUN_MS) {
                finish_rest_test();
            }
        }
    }

    if (gt_state == GT_WAIT && now - gt_t0 >= GT_WAIT_MS) {
        gt_state = GT_RUN;
        gt_t0 = now;
        gt_n = gt_an = 0;
        memset(gt_sum, 0, sizeof gt_sum);
        memset(gt_sq, 0, sizeof gt_sq);
        gt_asum = 0;
    } else if (gt_state == GT_RUN && now - gt_t0 >= GT_RUN_MS) {
        if (gt_n > 0) {
            finish_gyro_test();
        } else {
            gt_state = GT_IDLE;
            logmsg("Gyro-Ruhetest: keine Sensordaten");
        }
    }

    for (int tp = 0; tp < 1 && SDL_GetNumGamepadTouchpads(g) > 0; tp++) {
        int nf = SDL_min(SDL_GetNumGamepadTouchpadFingers(g, 0), 2);
        for (int f = 0; f < nf; f++) {
            bool down;
            float x, y, pr;
            if (SDL_GetGamepadTouchpadFinger(g, 0, f, &down, &x, &y, &pr) && down) {
                TouchTrail *t = &touch_trail[f];
                t->x[t->head] = x;
                t->y[t->head] = y;
                t->head = (t->head + 1) % TRAIL_LEN;
                if (t->n < TRAIL_LEN) {
                    t->n++;
                }
            }
        }
    }

    set_rumble(g, now);

    if (led_rainbow && now - led_last > 40) {
        led_last = now;
        led_color = hue((float)(now % 3000) / 3000.0f);
        SDL_SetGamepadLED(g, led_color.r, led_color.g, led_color.b);
    }
}

static void on_sensor(SDL_SensorType type, const float *d, Uint64 ts)
{
    if (ts == 0) {
        ts = SDL_GetTicksNS();
    }
    if (type == SDL_SENSOR_GYRO) {
        memcpy(gyro, d, sizeof gyro);
        have_gyro = true;
        float w[3];
        for (int i = 0; i < 3; i++) {
            w[i] = d[i] - (gbias_on ? gbias[i] : 0);
        }
        if (gyro_last_ts) {
            float dt = (float)(ts - gyro_last_ts) / 1e9f;
            if (dt > 0 && dt < 0.1f) {
                orient = qintegrate(orient, w, dt);
            }
        }
        gyro_last_ts = ts;
        for (int i = 0; i < 3; i++) {
            ggraph[gg_head][i] = w[i] * RAD2DEG;
        }
        gg_head = (gg_head + 1) % GG_LEN;
        if (gg_n < GG_LEN) {
            gg_n++;
        }
        if (gt_state == GT_RUN) {
            for (int i = 0; i < 3; i++) {
                double v = d[i] * RAD2DEG;
                gt_sum[i] += v;
                gt_sq[i] += v * v;
            }
            gt_n++;
        }
    } else if (type == SDL_SENSOR_ACCEL) {
        memcpy(accel, d, sizeof accel);
        have_accel = true;
        if (gt_state == GT_RUN) {
            gt_asum += sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / SDL_STANDARD_GRAVITY;
            gt_an++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Tab: Übersicht                                                      */
/* ------------------------------------------------------------------ */

static void kv(float x, float y, const char *k, SDL_Color c, const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    text(x, y, 2, C_DIM, k);
    text(x + 300, y, 2, c, buf);
}

static const char *power_text(SDL_Gamepad *g, char *buf, size_t n)
{
    int pct = -1;
    SDL_PowerState ps = SDL_GetGamepadPowerInfo(g, &pct);
    const char *s;
    switch (ps) {
    case SDL_POWERSTATE_ON_BATTERY: s = "Akku"; break;
    case SDL_POWERSTATE_NO_BATTERY: s = "kein Akku"; break;
    case SDL_POWERSTATE_CHARGING: s = "lädt"; break;
    case SDL_POWERSTATE_CHARGED: s = "voll geladen"; break;
    default: s = "unbekannt"; break;
    }
    if (pct >= 0) {
        snprintf(buf, n, "%s, %d %%", s, pct);
    } else {
        snprintf(buf, n, "%s", s);
    }
    return buf;
}

static void yesno(float x, float y, const char *k, bool v, const char *extra)
{
    kv(x, y, k, v ? C_GREEN : C_DIM, "%s%s", v ? "ja" : "nein", v && extra ? extra : "");
}

static void tab_info(SDL_Gamepad *g)
{
    float x = 30, y = 116;
    char buf[64];
    text(x, y, 2, C_ACCENT, "Controller");
    y += 30;
    kv(x, y, "Name", C_TEXT, "%.22s", SDL_GetGamepadName(g)); y += 22;
    kv(x, y, "Typ", C_TEXT, "%s", type_name(SDL_GetGamepadType(g))); y += 22;
    kv(x, y, "Hersteller/Produkt", C_TEXT, "%04x:%04x", SDL_GetGamepadVendor(g), SDL_GetGamepadProduct(g)); y += 22;
    const char *serial = SDL_GetGamepadSerial(g);
    kv(x, y, "Seriennummer", C_TEXT, "%.22s", serial && *serial ? serial : "-"); y += 22;
    Uint16 fw = SDL_GetGamepadFirmwareVersion(g);
    if (fw) {
        kv(x, y, "Firmware", C_TEXT, "%04x", fw);
    } else {
        kv(x, y, "Firmware", C_TEXT, "-");
    }
    y += 22;
    SDL_JoystickConnectionState cs = SDL_GetGamepadConnectionState(g);
    kv(x, y, "Verbindung", C_TEXT, "%s", cs == SDL_JOYSTICK_CONNECTION_WIRED ? "Kabel (USB)"
                                       : cs == SDL_JOYSTICK_CONNECTION_WIRELESS ? "kabellos" : "unbekannt");
    y += 22;
    kv(x, y, "Stromversorgung", C_TEXT, "%s", power_text(g, buf, sizeof buf)); y += 22;
    int pi = SDL_GetGamepadPlayerIndex(g);
    if (pi >= 0) {
        kv(x, y, "Spieler-Nummer", C_TEXT, "%d", pi + 1);
    } else {
        kv(x, y, "Spieler-Nummer", C_TEXT, "-");
    }
    y += 40;

    text(x, y, 2, C_ACCENT, "Fähigkeiten");
    y += 30;
    int nb = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        nb += SDL_GamepadHasButton(g, (SDL_GamepadButton)b);
    }
    kv(x, y, "Tasten", C_TEXT, "%d", nb); y += 22;
    yesno(x, y, "Analoge Trigger", SDL_GamepadHasAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER), NULL); y += 22;
    yesno(x, y, "Vibration", cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN), NULL); y += 22;
    yesno(x, y, "Trigger-Vibration", cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN), NULL); y += 22;
    bool gy = SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO), ac = SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL);
    if (gy) {
        snprintf(buf, sizeof buf, " (%.0f Hz)", SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO));
    }
    yesno(x, y, "Gyrosensor", gy, buf); y += 22;
    if (ac) {
        snprintf(buf, sizeof buf, " (%.0f Hz)", SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_ACCEL));
    }
    yesno(x, y, "Beschleunigung", ac, buf); y += 22;
    int ntp = SDL_GetNumGamepadTouchpads(g);
    yesno(x, y, "Touchpad", ntp > 0, NULL); y += 22;
    yesno(x, y, "RGB-LED", cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN), NULL); y += 22;
    yesno(x, y, "Spieler-LED", cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN), NULL); y += 22;

    float rx = 720;
    y = 116;
    text(rx, y, 2, C_ACCENT, "Verbundene Controller (anklicken)");
    y += 30;
    for (int i = 0; i < npads; i++) {
        char label[64];
        snprintf(label, sizeof label, "%d: %.30s", i + 1, SDL_GetGamepadName(pads[i]));
        if (ui_button(rx, y, 544, 30, label, i == act) && i != act) {
            select_pad(i);
            return;
        }
        y += 36;
    }
    y += 20;
    text(rx, y, 2, C_ACCENT, "Ereignisse");
    draw_log(rx, y + 30, 12, 33);
}

/* ------------------------------------------------------------------ */
/* Tab: Tasten                                                         */
/* ------------------------------------------------------------------ */

static SDL_Color btn_col(SDL_Gamepad *g, SDL_GamepadButton b)
{
    if (SDL_GetGamepadButton(g, b)) {
        return C_PRESS;
    }
    return seen[b] ? C_GREEN_DIM : C_PANEL3;
}

static void draw_btn_rect(SDL_Gamepad *g, SDL_GamepadButton b, float x, float y, float w, float h, const char *label)
{
    if (!SDL_GamepadHasButton(g, b)) {
        return;
    }
    fill(x, y, w, h, btn_col(g, b));
    outline(x, y, w, h, C_LINE);
    if (label) {
        text_center(x + w / 2, y + (h - 8) / 2, 1, C_TEXT, label);
    }
}

static void draw_face(SDL_Gamepad *g, SDL_GamepadButton b, float x, float y)
{
    if (!SDL_GamepadHasButton(g, b)) {
        return;
    }
    disc(x, y, 18, btn_col(g, b));
    circle(x, y, 18, C_LINE);
    SDL_GamepadButtonLabel l = SDL_GetGamepadButtonLabel(g, b);
    switch (l) {
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        line(x - 7, y - 7, x + 7, y + 7, C_TEXT);
        line(x - 7, y + 7, x + 7, y - 7, C_TEXT);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        circle(x, y, 8, C_TEXT);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        outline(x - 7, y - 7, 14, 14, C_TEXT);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
        line(x, y - 8, x - 8, y + 6, C_TEXT);
        line(x - 8, y + 6, x + 8, y + 6, C_TEXT);
        line(x + 8, y + 6, x, y - 8, C_TEXT);
        break;
    default:
        text_center(x, y - 8, 2, C_TEXT, btn_name(g, b));
        break;
    }
}

static void draw_mini_stick(SDL_Gamepad *g, int s, float cx, float cy)
{
    SDL_GamepadButton click = s ? SDL_GAMEPAD_BUTTON_RIGHT_STICK : SDL_GAMEPAD_BUTTON_LEFT_STICK;
    disc(cx, cy, 42, C_PANEL);
    circle(cx, cy, 42, seen[click] ? C_GREEN : C_LINE);
    disc(cx + st_x[s] * 30, cy + st_y[s] * 30, 12, SDL_GetGamepadButton(g, click) ? C_PRESS : C_TEXT);
}

static void draw_trigger_bar(SDL_Gamepad *g, int s, float x, float y, float w, float h)
{
    if (!SDL_GamepadHasAxis(g, s ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) {
        return;
    }
    fill(x, y, w, h, C_PANEL);
    fill(x, y, w * trig[s], h, trig[s] > 0.02f ? C_PRESS : C_PANEL3);
    outline(x, y, w, h, seen_trig[s] ? C_GREEN : C_LINE);
    textf(x + 6, y + (h - 16) / 2, 2, C_TEXT, "%s %3.0f%%", trig_name(g, s), trig[s] * 100);
}

static void check_item(float x, float y, bool done, bool pressed, const char *label)
{
    fill(x, y, 16, 16, pressed ? C_PRESS : done ? C_GREEN : C_PANEL2);
    outline(x, y, 16, 16, C_LINE);
    if (done) {
        line(x + 3, y + 8, x + 7, y + 12, C_BG);
        line(x + 7, y + 12, x + 13, y + 4, C_BG);
    }
    text(x + 24, y, 2, done ? C_TEXT : C_DIM, label);
}

static void tab_buttons(SDL_Gamepad *g)
{
    bool ps = is_ps(SDL_GetGamepadType(g));

    /* Gehäuse */
    disc(410, 290, 80, C_PANEL2);
    disc(870, 290, 80, C_PANEL2);
    disc(450, 440, 80, C_PANEL2);
    disc(830, 440, 80, C_PANEL2);
    fill(410, 210, 460, 230, C_PANEL2);

    draw_trigger_bar(g, 0, 400, 126, 160, 28);
    draw_trigger_bar(g, 1, 720, 126, 160, 28);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 400, 164, 160, 26, btn_name(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 720, 164, 160, 26, btn_name(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));

    float lsx = ps ? 560 : 470, lsy = ps ? 390 : 290;
    float dpx = ps ? 470 : 560, dpy = ps ? 290 : 390;
    draw_mini_stick(g, 0, lsx, lsy);
    draw_mini_stick(g, 1, 720, 390);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_DPAD_UP, dpx - 13, dpy - 39, 26, 26, NULL);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN, dpx - 13, dpy + 13, 26, 26, NULL);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT, dpx - 39, dpy - 13, 26, 26, NULL);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, dpx + 13, dpy - 13, 26, 26, NULL);
    fill(dpx - 13, dpy - 13, 26, 26, C_PANEL3);

    draw_face(g, SDL_GAMEPAD_BUTTON_NORTH, 810, 250);
    draw_face(g, SDL_GAMEPAD_BUTTON_SOUTH, 810, 330);
    draw_face(g, SDL_GAMEPAD_BUTTON_WEST, 770, 290);
    draw_face(g, SDL_GAMEPAD_BUTTON_EAST, 850, 290);

    if (SDL_GamepadHasButton(g, SDL_GAMEPAD_BUTTON_TOUCHPAD)) {
        draw_btn_rect(g, SDL_GAMEPAD_BUTTON_TOUCHPAD, 575, 212, 130, 60, "Touchpad");
        draw_btn_rect(g, SDL_GAMEPAD_BUTTON_BACK, 538, 218, 28, 18, NULL);
        draw_btn_rect(g, SDL_GAMEPAD_BUTTON_START, 714, 218, 28, 18, NULL);
    } else {
        draw_btn_rect(g, SDL_GAMEPAD_BUTTON_BACK, 585, 250, 34, 20, NULL);
        draw_btn_rect(g, SDL_GAMEPAD_BUTTON_START, 661, 250, 34, 20, NULL);
    }
    if (SDL_GamepadHasButton(g, SDL_GAMEPAD_BUTTON_GUIDE)) {
        disc(640, 308, 14, btn_col(g, SDL_GAMEPAD_BUTTON_GUIDE));
        circle(640, 308, 14, C_LINE);
    }
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_MISC1, 628, 332, 24, 12, NULL);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_MISC2, 628, 350, 24, 12, NULL);
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, 420, 540, 100, 22, "Paddle L1");
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, 420, 568, 100, 22, "Paddle L2");
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, 760, 540, 100, 22, "Paddle R1");
    draw_btn_rect(g, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, 760, 568, 100, 22, "Paddle R2");
    for (int i = 0; i < 4; i++) {
        SDL_GamepadButton b = (SDL_GamepadButton)(SDL_GAMEPAD_BUTTON_MISC3 + i);
        char l[8];
        snprintf(l, sizeof l, "Z%d", i + 3);
        draw_btn_rect(g, b, 560 + i * 42, 600, 36, 22, l);
    }

    text(20, 660, 2, C_PRESS, "orange = gedrückt");
    text(20, 682, 2, C_GREEN, "grün = getestet");

    /* Checkliste */
    int total = 0, done = 0;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        if (SDL_GamepadHasButton(g, (SDL_GamepadButton)b)) {
            total++;
            done += seen[b];
        }
    }
    for (int s = 0; s < 2; s++) {
        if (SDL_GamepadHasAxis(g, s ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) {
            total++;
            done += seen_trig[s];
        }
    }
    float x = 950, y = 112;
    textf(x, y, 2, done == total ? C_GREEN : C_ACCENT, "Getestet: %d / %d", done, total);
    y += 28;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
        if (SDL_GamepadHasButton(g, (SDL_GamepadButton)b)) {
            check_item(x, y, seen[b], SDL_GetGamepadButton(g, (SDL_GamepadButton)b), btn_name(g, (SDL_GamepadButton)b));
            y += 22;
        }
    }
    for (int s = 0; s < 2; s++) {
        if (SDL_GamepadHasAxis(g, s ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) {
            char l[32];
            snprintf(l, sizeof l, "%s ganz drücken", trig_name(g, s));
            check_item(x, y, seen_trig[s], trig[s] > 0.02f, l);
            y += 22;
        }
    }

    text(20, 112, 2, C_ACCENT, "Letzte Eingaben");
    draw_log(20, 140, 24, 19);
    if (done == total && total > 0) {
        text_center(640, 684, 3, C_GREEN, "Alle Tasten getestet!");
    }
    text(20, 726, 2, C_DIM, "[R] zurücksetzen");
}

/* ------------------------------------------------------------------ */
/* Tab: Sticks                                                         */
/* ------------------------------------------------------------------ */

static void draw_stick_view(SDL_Gamepad *g, int s, float cx, float cy, float r)
{
    disc(cx, cy, r, C_PANEL);
    circle(cx, cy, r, C_LINE);
    circle(cx, cy, r * 0.1f, C_PANEL3);
    line(cx - r, cy, cx + r, cy, C_PANEL3);
    line(cx, cy - r, cx, cy + r, C_PANEL3);
    Trail *t = &trail[s];
    for (int i = 0; i < t->n; i++) {
        int idx = (t->head - 1 - i + TRAIL_LEN) % TRAIL_LEN;
        Uint8 a = (Uint8)(255 - i * 255 / TRAIL_LEN);
        SDL_FRect p = {cx + t->x[idx] * r - 1, cy + t->y[idx] * r - 1, 3, 3};
        SDL_SetRenderDrawColor(ren, C_ACCENT.r, C_ACCENT.g, C_ACCENT.b, a);
        SDL_RenderFillRect(ren, &p);
    }
    float px = cx + st_x[s] * r, py = cy + st_y[s] * r;
    line(cx, cy, px, py, C_DIM);
    bool click = SDL_GetGamepadButton(g, s ? SDL_GAMEPAD_BUTTON_RIGHT_STICK : SDL_GAMEPAD_BUTTON_LEFT_STICK);
    disc(px, py, 10, click ? C_PRESS : C_TEXT);
}

static void tab_sticks(SDL_Gamepad *g)
{
    const char *names[2] = {"Linker Stick", "Rechter Stick"};
    float cxs[2] = {290, 990};
    for (int s = 0; s < 2; s++) {
        float cx = cxs[s], cy = 340;
        text_center(cx, 112, 2, C_ACCENT, names[s]);
        draw_stick_view(g, s, cx, cy, 190);
        SDL_GamepadAxis ax = s ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_LEFTX;
        SDL_GamepadAxis ay = s ? SDL_GAMEPAD_AXIS_RIGHTY : SDL_GAMEPAD_AXIS_LEFTY;
        float x0 = cx - 190, y0 = 550;
        textf(x0, y0, 2, C_TEXT, "X: %+6.3f  (roh %6d)", st_x[s], SDL_GetGamepadAxis(g, ax));
        textf(x0, y0 + 22, 2, C_TEXT, "Y: %+6.3f  (roh %6d)", st_y[s], SDL_GetGamepadAxis(g, ay));
        float mag = hypotf(st_x[s], st_y[s]);
        float ang = atan2f(-st_y[s], st_x[s]) * RAD2DEG;
        textf(x0, y0 + 44, 2, C_TEXT, "Auslenkung: %5.1f %%", mag * 100);
        textf(x0, y0 + 66, 2, C_TEXT, "Winkel:     %5.0f°", ang < 0 ? ang + 360 : ang);
        bool click = SDL_GetGamepadButton(g, s ? SDL_GAMEPAD_BUTTON_RIGHT_STICK : SDL_GAMEPAD_BUTTON_LEFT_STICK);
        textf(x0, y0 + 88, 2, click ? C_PRESS : C_DIM, "Stick-Klick: %s", click ? "gedrückt" : "-");
    }
    /* Trigger als senkrechte Balken in der Mitte */
    for (int s = 0; s < 2; s++) {
        if (!SDL_GamepadHasAxis(g, s ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) {
            continue;
        }
        float x = s ? 680 : 560, y = 180, h = 320, w = 40;
        fill(x, y, w, h, C_PANEL);
        fill(x, y + h * (1 - trig[s]), w, h * trig[s], C_PRESS);
        outline(x, y, w, h, C_LINE);
        text_center(x + w / 2, y - 26, 2, C_TEXT, trig_name(g, s));
        textf(x - 4, y + h + 10, 2, C_TEXT, "%3.0f%%", trig[s] * 100);
    }
    text_center(640, 600, 2, C_DIM, "Kreis innen = 10 %");
    text_center(640, 622, 2, C_DIM, "Y+ = unten (SDL)");
}

/* ------------------------------------------------------------------ */
/* Tab: Stickdrift                                                     */
/* ------------------------------------------------------------------ */

static void circle_stats(int s, float *cover, float *mean, float *err, float *minr)
{
    int n = 0;
    float sum = 0, e = 0, mn = 10;
    for (int i = 0; i < CT_BINS; i++) {
        float r = ct_bin[s][i];
        if (r <= 0) {
            continue;
        }
        n++;
        sum += r;
        e += fabsf(r - 1);
        mn = SDL_min(mn, r);
    }
    *cover = 100.0f * n / CT_BINS;
    *mean = n ? sum / n * 100 : 0;
    *err = n ? e / n * 100 : 0;
    *minr = n ? mn * 100 : 0;
}

static void drift_panel(int s, float px)
{
    const char *names[2] = {"Linker Stick", "Rechter Stick"};
    float py = 150;
    fill(px, py, 616, 610, C_PANEL);
    text(px + 16, py + 10, 2, C_ACCENT, names[s]);

    /* Zoom auf die Mitte: Rand = 15 % Auslenkung */
    const float zoom = 15.0f;
    float zx = px + 150, zy = py + 160, zr = 110;
    disc(zx, zy, zr, C_BG);
    circle(zx, zy, zr * DRIFT_OK / zoom, C_GREEN);
    circle(zx, zy, zr * DRIFT_WARN / zoom, C_YELLOW);
    circle(zx, zy, zr, C_LINE);
    line(zx - zr, zy, zx + zr, zy, C_PANEL3);
    line(zx, zy - zr, zx, zy + zr, C_PANEL3);
    float vx = st_x[s] * 100 / zoom, vy = st_y[s] * 100 / zoom;
    float vm = hypotf(vx, vy);
    bool clipped = vm > 1;
    if (clipped) {
        vx /= vm;
        vy /= vm;
    }
    disc(zx + vx * zr, zy + vy * zr, 6, clipped ? C_RED : C_TEXT);
    text_center(zx, zy + zr + 8, 1, C_DIM, "Zoom: Rand = 15 %");
    float live = hypotf(st_x[s], st_y[s]) * 100;
    textf(px + 16, py + 300, 2, rating_color(drift_rating(live)), "Live: %5.2f %%", live);

    /* Ruhetest-Ergebnis */
    float ty = py + 330;
    text(px + 16, ty, 2, C_DIM, "Ruhetest");
    if (rt_state == RT_DONE) {
        int r = drift_rating(rt_max[s]);
        textf(px + 16, ty + 24, 2, C_TEXT, "Versatz:  %5.2f %%", rt_off[s]);
        textf(px + 16, ty + 46, 2, C_TEXT, "Rauschen: %5.2f %%", rt_noise[s]);
        textf(px + 16, ty + 68, 2, rating_color(r), "Maximum:  %5.2f %%", rt_max[s]);
        textf(px + 16, ty + 160, 2, C_DIM, "Ruhe-Mitte: x %+.2f %%  y %+.2f %%", rt_mx[s] * 100, rt_my[s] * 100);
        const char *verdict[3] = {"OK - kein Drift erkennbar", "Leichter Drift (Deadzone fängt ab)", "Drift! Stick kehrt nicht zur Mitte"};
        fill(px + 16, ty + 120, 584, 30, rating_color(r));
        text(px + 26, ty + 127, 2, C_BG, verdict[r]);
    } else {
        text(px + 16, ty + 24, 2, C_DIM, "noch nicht gemessen");
    }

    /* Kreistest */
    float cx = px + 460, cy = py + 160, cr = 110;
    disc(cx, cy, cr * 1.2f, C_BG);
    circle(cx, cy, cr, C_LINE);
    line(cx - cr, cy, cx + cr, cy, C_PANEL3);
    line(cx, cy - cr, cx, cy + cr, C_PANEL3);
    float lx = 0, ly = 0;
    bool prev = false;
    for (int i = 0; i <= CT_BINS; i++) {
        int b = i % CT_BINS;
        float r = ct_bin[s][b];
        if (r <= 0) {
            prev = false;
            continue;
        }
        float a = ((b + 0.5f) / CT_BINS) * 2 * PI_F - PI_F;
        float x = cx + cosf(a) * r * cr, y = cy + sinf(a) * r * cr;
        if (prev) {
            line(lx, ly, x, y, C_ACCENT);
        }
        lx = x;
        ly = y;
        prev = true;
    }
    disc(cx + st_x[s] * cr, cy + st_y[s] * cr, 5, C_TEXT);

    float cover, mean, err, minr;
    circle_stats(s, &cover, &mean, &err, &minr);
    float bx = px + 326;
    if (ui_button(bx, py + 300, 280, 30, ct_run[s] ? "Kreistest beenden" : "Kreistest starten", ct_run[s])) {
        if (!ct_run[s]) {
            memset(ct_bin[s], 0, sizeof ct_bin[s]);
            logmsg("Kreistest %s gestartet", s ? "rechts" : "links");
        }
        ct_run[s] = !ct_run[s];
    }
    if (cover > 0) {
        textf(bx, ty + 24, 2, cover >= 99 ? C_GREEN : C_TEXT, "Abdeckung: %3.0f %%", cover);
        textf(bx, ty + 46, 2, C_TEXT, "Radius:    %3.0f %%", mean);
        textf(bx, ty + 68, 2, minr < 90 ? C_RED : C_TEXT, "Min.:      %3.0f %%", minr);
        int r = err < 8 ? 0 : err < 15 ? 1 : 2;
        textf(bx, ty + 90, 2, rating_color(r), "Fehler:  %5.1f %%", err);
    } else if (ct_run[s]) {
        text(bx, ty + 24, 2, C_YELLOW, "Stick am Rand");
        text(bx, ty + 46, 2, C_YELLOW, "kreisen lassen");
    }
    if (!ct_run[s] && cover >= 99) {
        const char *msg = minr < 90 ? "Rand wird nicht überall erreicht" : err < 15 ? "Kreistest: Ausschlag in Ordnung" : "Kreistest: Ausschlag ungleichmäßig";
        text(px + 16, py + 570, 2, minr < 90 ? C_RED : err < 15 ? C_GREEN : C_YELLOW, msg);
    }
}

static void tab_drift(void)
{
    Uint64 now = SDL_GetTicks();
    const char *status = "Ruhetest: beide Sticks 5 s lang nicht berühren.";
    SDL_Color sc = C_DIM;
    char buf[96];
    if (rt_state == RT_WAIT) {
        snprintf(buf, sizeof buf, "Sticks LOSLASSEN! Messung beginnt in %.0f s...", ceilf((RT_WAIT_MS - (float)(now - rt_t0)) / 1000));
        status = buf;
        sc = C_YELLOW;
    } else if (rt_state == RT_RUN) {
        snprintf(buf, sizeof buf, "Messe... %.1f s - nicht berühren!", (RT_RUN_MS - (float)(now - rt_t0)) / 1000);
        status = buf;
        sc = C_YELLOW;
    } else if (rt_state == RT_ABORT) {
        status = "Abgebrochen: Stick wurde bewegt. Bitte erneut starten.";
        sc = C_RED;
    } else if (rt_state == RT_DONE) {
        status = "Ruhetest abgeschlossen.";
        sc = C_GREEN;
    }
    text(20, 116, 2, sc, status);
    bool busy = rt_state == RT_WAIT || rt_state == RT_RUN;
    if (ui_button(950, 108, 310, 32, busy ? "Abbrechen" : "Ruhetest starten", busy)) {
        if (busy) {
            rt_state = RT_IDLE;
        } else {
            rt_state = RT_WAIT;
            rt_t0 = now;
        }
    }
    drift_panel(0, 16);
    drift_panel(1, 648);
}

/* ------------------------------------------------------------------ */
/* Tab: Vibration                                                      */
/* ------------------------------------------------------------------ */

static void motor_view(float x, float y, const char *label, Uint16 v)
{
    float f = v / 65535.0f;
    float r = 30 + f * 30;
    Uint64 now = SDL_GetTicks();
    float wob = f > 0 ? sinf((float)now / 25.0f) * 4 * f : 0;
    disc(x + wob, y, r, f > 0 ? C_PRESS : C_PANEL2);
    circle(x, y, 60, C_LINE);
    text_center(x, y + 70, 2, C_TEXT, label);
    char buf[16];
    snprintf(buf, sizeof buf, "%3.0f %%", f * 100);
    text_center(x, y + 92, 2, C_DIM, buf);
}

static void start_rumble(int mode)
{
    rm_mode = mode;
    rm_t0 = SDL_GetTicks();
    rm_err[0] = 0;
}

static void tab_rumble(SDL_Gamepad *g)
{
    bool has = cap(g, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN);
    bool has_t = cap(g, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN);
    float x = 40, y = 116;
    if (!has) {
        text(x, y, 2, C_YELLOW, "Dieser Controller meldet keine Vibration. Du kannst es trotzdem versuchen.");
        y += 30;
    }
    text(x, y, 2, C_TEXT, "Stärke");
    ui_slider(x + 120, y - 2, 400, &rm_int);
    y += 50;

    if (ui_button(x, y, 310, 40, "Links (tief) 1,5 s", rm_mode == RM_LOW)) start_rumble(RM_LOW);
    if (ui_button(x + 330, y, 310, 40, "Rechts (hoch) 1,5 s", rm_mode == RM_HIGH)) start_rumble(RM_HIGH);
    y += 54;
    if (ui_button(x, y, 310, 40, "Beide 1,5 s", rm_mode == RM_BOTH)) start_rumble(RM_BOTH);
    if (ui_button(x + 330, y, 310, 40, rm_mode == RM_CONT ? "Dauerbetrieb AUS" : "Dauerbetrieb AN", rm_mode == RM_CONT)) {
        start_rumble(rm_mode == RM_CONT ? RM_NONE : RM_CONT);
    }
    y += 54;
    if (ui_button(x, y, 310, 40, "Wechsel L/R (4 s)", rm_mode == RM_ALT)) start_rumble(RM_ALT);
    if (ui_button(x + 330, y, 310, 40, "Rampe 0-100 % (3 s)", rm_mode == RM_RAMP)) start_rumble(RM_RAMP);
    y += 54;
    if (ui_button(x, y, 640, 40, "Stopp", false)) start_rumble(RM_NONE);
    y += 70;

    text(x, y, 2, has_t ? C_ACCENT : C_DIM, has_t ? "Trigger-Vibration" : "Trigger-Vibration: nicht vorhanden");
    y += 30;
    if (has_t) {
        if (ui_button(x, y, 200, 40, "Trigger links", rm_mode == RM_TRIG_L)) start_rumble(RM_TRIG_L);
        if (ui_button(x + 210, y, 200, 40, "Trigger rechts", rm_mode == RM_TRIG_R)) start_rumble(RM_TRIG_R);
        if (ui_button(x + 420, y, 200, 40, "Beide", rm_mode == RM_TRIG_B)) start_rumble(RM_TRIG_B);
        y += 54;
    }
    if (SDL_GetGamepadType(g) == SDL_GAMEPAD_TYPE_PS5 && !has_t) {
        text(x, y, 2, C_DIM, "Die adaptiven Trigger der DualSense steuert SDL nicht an.");
        y += 24;
    }
    if (rm_err[0]) {
        text(x, y + 10, 2, C_RED, rm_err);
    }

    motor_view(850, 300, "Links (tief)", rm_cur[0]);
    motor_view(1110, 300, "Rechts (hoch)", rm_cur[1]);
    if (has_t) {
        motor_view(850, 560, "Trigger L", rm_cur[2]);
        motor_view(1110, 560, "Trigger R", rm_cur[3]);
    }
    text_center(980, 700, 2, C_DIM, "Spürst du beide Motoren getrennt?");
}

/* ------------------------------------------------------------------ */
/* Tab: Gyro                                                           */
/* ------------------------------------------------------------------ */

static float view_cx, view_cy;

static void project(const float p[3], float out[2])
{
    float r[3];
    qrot(orient, p, r);
    const float a = 0.45f; /* Kamera schaut leicht von oben */
    float y = r[1] * cosf(a) - r[2] * sinf(a);
    float z = r[1] * sinf(a) + r[2] * cosf(a);
    float d = 4.0f - z;
    out[0] = view_cx + 620 * r[0] / d;
    out[1] = view_cy - 620 * y / d;
}

static void box3d(float cx, float cy, float cz, float hx, float hy, float hz, SDL_Color c)
{
    float v[8][2];
    for (int i = 0; i < 8; i++) {
        float p[3] = {cx + ((i & 1) ? hx : -hx), cy + ((i & 2) ? hy : -hy), cz + ((i & 4) ? hz : -hz)};
        project(p, v[i]);
    }
    for (int i = 0; i < 8; i++) {
        for (int bit = 1; bit < 8; bit <<= 1) {
            if (!(i & bit)) {
                line(v[i][0], v[i][1], v[i | bit][0], v[i | bit][1], c);
            }
        }
    }
}

static void seg3d(const float a[3], const float b[3], SDL_Color c)
{
    float pa[2], pb[2];
    project(a, pa);
    project(b, pb);
    line(pa[0], pa[1], pb[0], pb[1], c);
}

static void draw_controller3d(void)
{
    /* SDL-Koordinaten: +X rechts, +Y oben, -Z vom Spieler weg */
    box3d(0, 0, 0, 1.0f, 0.15f, 0.45f, C_TEXT);
    box3d(-0.72f, -0.02f, 0.62f, 0.26f, 0.13f, 0.2f, C_DIM);
    box3d(0.72f, -0.02f, 0.62f, 0.26f, 0.13f, 0.2f, C_DIM);
    box3d(-0.65f, 0.05f, -0.52f, 0.22f, 0.08f, 0.08f, C_ACCENT);
    box3d(0.65f, 0.05f, -0.52f, 0.22f, 0.08f, 0.08f, C_ACCENT);
    float s1a[3] = {-0.45f, 0.15f, 0.05f}, s1b[3] = {-0.45f, 0.4f, 0.05f};
    float s2a[3] = {0.45f, 0.15f, 0.05f}, s2b[3] = {0.45f, 0.4f, 0.05f};
    seg3d(s1a, s1b, C_PRESS);
    seg3d(s2a, s2b, C_PRESS);
    float fa[3] = {0, 0.15f, -0.45f}, fb[3] = {0, 0.15f, -0.95f};
    seg3d(fa, fb, C_GREEN);
    float lp[2];
    project(fb, lp);
    text(lp[0] + 6, lp[1] - 8, 2, C_GREEN, "vorne");
}

static void hbar_center(float x, float y, float w, float h, float v, float range, SDL_Color c)
{
    fill(x, y, w, h, C_PANEL);
    float f = SDL_clamp(v / range, -1.0f, 1.0f);
    float mid = x + w / 2;
    if (f >= 0) {
        fill(mid, y, w / 2 * f, h, c);
    } else {
        fill(mid + w / 2 * f, y, -w / 2 * f, h, c);
    }
    line(mid, y, mid, y + h, C_LINE);
}

static void tab_gyro(SDL_Gamepad *g)
{
    bool hg = SDL_GamepadHasSensor(g, SDL_SENSOR_GYRO), ha = SDL_GamepadHasSensor(g, SDL_SENSOR_ACCEL);
    if (!hg && !ha) {
        text_center(640, 380, 2, C_YELLOW, "Dieser Controller meldet keinen Gyro- oder Beschleunigungssensor.");
        text_center(640, 410, 2, C_DIM, "8BitDo-Controller haben oft nur im Switch-Modus einen Gyro.");
        return;
    }
    Uint64 now = SDL_GetTicks();

    /* 3D-Ansicht */
    fill(16, 104, 620, 420, C_PANEL);
    view_cx = 326;
    view_cy = 314;
    if (hg) {
        draw_controller3d();
    }
    text(26, 112, 2, C_DIM, "3D-Ansicht aus dem Gyro");
    text(26, 496, 1, C_DIM, "Controller normal halten und [R] drücken, um die Ausrichtung zurückzusetzen.");

    /* Graph */
    float gx = 16, gy = 534, gw = 620, gh = 226;
    fill(gx, gy, gw, gh, C_PANEL);
    float range = 5;
    for (int i = 0; i < gg_n; i++) {
        for (int a = 0; a < 3; a++) {
            range = SDL_max(range, fabsf(ggraph[i][a]));
        }
    }
    line(gx, gy + gh / 2, gx + gw, gy + gh / 2, C_PANEL3);
    for (int a = 0; a < 3; a++) {
        float px = 0, py = 0;
        for (int i = 0; i < gg_n; i++) {
            int idx = (gg_head - gg_n + i + GG_LEN) % GG_LEN;
            float x = gx + gw * i / (float)(GG_LEN - 1);
            float y = gy + gh / 2 - ggraph[idx][a] / range * (gh / 2 - 4);
            if (i > 0) {
                line(px, py, x, y, C_AX[a]);
            }
            px = x;
            py = y;
        }
    }
    textf(gx + 8, gy + 6, 2, C_DIM, "Gyro-Verlauf  +/-%.0f °/s", range);

    /* Messwerte */
    float x = 652, y = 112;
    text(x, y, 2, C_ACCENT, "Gyro (°/s)");
    const char *axn[3] = {"X (Nicken)", "Y (Gieren)", "Z (Rollen)"};
    for (int a = 0; a < 3; a++) {
        float v = (gyro[a] - (gbias_on ? gbias[a] : 0)) * RAD2DEG;
        y += 24;
        text(x, y, 2, C_AX[a], axn[a]);
        textf(x + 180, y, 2, C_TEXT, "%+8.2f", hg && have_gyro ? v : 0.0f);
        hbar_center(x + 340, y, 250, 16, v, 360, C_AX[a]);
    }
    y += 40;
    text(x, y, 2, C_ACCENT, "Beschleunigung (m/s^2)");
    for (int a = 0; a < 3; a++) {
        y += 24;
        textf(x, y, 2, C_AX[a], "%c", "XYZ"[a]);
        textf(x + 180, y, 2, C_TEXT, "%+8.2f", ha && have_accel ? accel[a] : 0.0f);
        hbar_center(x + 340, y, 250, 16, accel[a], 20, C_AX[a]);
    }
    y += 24;
    float amag = sqrtf(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]) / SDL_STANDARD_GRAVITY;
    textf(x, y, 2, C_TEXT, "Gesamt: %.2f g", amag);
    y += 24;
    textf(x, y, 2, C_DIM, "Datenrate: Gyro %.0f Hz, Accel %.0f Hz", hg ? SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_GYRO) : 0.0f,
          ha ? SDL_GetGamepadSensorDataRate(g, SDL_SENSOR_ACCEL) : 0.0f);
    if (hg && !have_gyro) {
        y += 24;
        text(x, y, 2, C_YELLOW, "Noch keine Gyro-Daten empfangen.");
    }
    y += 36;

    if (ui_button(x, y, 300, 34, "Ausrichtung zurück", false)) {
        reset_gyro();
    }
    bool busy = gt_state == GT_WAIT || gt_state == GT_RUN;
    if (hg && ui_button(x + 310, y, 300, 34, busy ? "Abbrechen" : "Gyro-Ruhetest", busy)) {
        if (busy) {
            gt_state = GT_IDLE;
        } else {
            gt_state = GT_WAIT;
            gt_t0 = now;
        }
    }
    y += 44;
    if (hg && ui_button(x, y, 300, 34, gbias_on ? "Bias-Korrektur an" : "Bias-Korrektur aus", gbias_on)) {
        gbias_on = !gbias_on;
        gyro_last_ts = 0;
    }
    y += 50;

    if (gt_state == GT_WAIT) {
        textf(x, y, 2, C_YELLOW, "Controller flach hinlegen, nicht berühren... %.0f", ceilf((GT_WAIT_MS - (float)(now - gt_t0)) / 1000));
    } else if (gt_state == GT_RUN) {
        textf(x, y, 2, C_YELLOW, "Messe... %.1f s", (GT_RUN_MS - (float)(now - gt_t0)) / 1000);
    } else if (gt_state == GT_DONE) {
        float worst = SDL_max(fabsf(gt_bias[0]), SDL_max(fabsf(gt_bias[1]), fabsf(gt_bias[2])));
        int r = worst < 1.0f ? 0 : worst < 3.0f ? 1 : 2;
        text(x, y, 2, C_ACCENT, "Gyro-Ruhetest");
        textf(x, y + 24, 2, rating_color(r), "Bias:     %+5.2f %+5.2f %+5.2f °/s", gt_bias[0], gt_bias[1], gt_bias[2]);
        textf(x, y + 46, 2, C_TEXT, "Rauschen:  %5.2f  %5.2f  %5.2f °/s", gt_noise[0], gt_noise[1], gt_noise[2]);
        if (ha) {
            bool aok = gt_amag > 0.9f && gt_amag < 1.1f;
            textf(x, y + 68, 2, aok ? C_GREEN : C_YELLOW, "Schwerkraft: %.3f g (soll ~1)", gt_amag);
        }
        const char *verdict[3] = {"Gyro OK - sehr geringer Drift", "Gyro leicht ungenau", "Gyro driftet deutlich"};
        fill(x, y + 96, 600, 30, rating_color(r));
        text(x + 10, y + 103, 2, C_BG, verdict[r]);
        text(x, y + 136, 2, C_DIM, "Bias wird jetzt automatisch abgezogen.");
    } else {
        text(x, y, 2, C_DIM, "Gyro-Ruhetest: Knopf drücken,");
        text(x, y + 22, 2, C_DIM, "Controller flach hinlegen, 7 s warten.");
    }
}

/* ------------------------------------------------------------------ */
/* Tab: Extras                                                         */
/* ------------------------------------------------------------------ */

static void tab_extras(SDL_Gamepad *g)
{
    /* Touchpad */
    fill(16, 104, 620, 400, C_PANEL);
    text(30, 114, 2, C_ACCENT, "Touchpad");
    if (SDL_GetNumGamepadTouchpads(g) > 0) {
        float tx = 46, ty = 160, tw = 560, th = 280;
        fill(tx, ty, tw, th, C_BG);
        outline(tx, ty, tw, th, C_LINE);
        SDL_Color fc[2] = {C_ACCENT, C_PRESS};
        for (int f = 0; f < 2; f++) {
            TouchTrail *t = &touch_trail[f];
            for (int i = 0; i < t->n; i++) {
                int idx = (t->head - 1 - i + TRAIL_LEN) % TRAIL_LEN;
                SDL_FRect p = {tx + t->x[idx] * tw - 1, ty + t->y[idx] * th - 1, 3, 3};
                SDL_SetRenderDrawColor(ren, fc[f].r, fc[f].g, fc[f].b, (Uint8)(200 - i * 200 / TRAIL_LEN));
                SDL_RenderFillRect(ren, &p);
            }
        }
        int nf = SDL_GetNumGamepadTouchpadFingers(g, 0);
        for (int f = 0; f < nf && f < 2; f++) {
            bool down;
            float x, y, pr;
            if (SDL_GetGamepadTouchpadFinger(g, 0, f, &down, &x, &y, &pr) && down) {
                disc(tx + x * tw, ty + y * th, 16, fc[f]);
                textf(tx + 8, ty + th + 12 + f * 22, 2, fc[f], "Finger %d: x %.2f  y %.2f", f + 1, x, y);
            }
        }
        if (SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_TOUCHPAD)) {
            outline(tx - 3, ty - 3, tw + 6, th + 6, C_PRESS);
        }
    } else {
        text(30, 150, 2, C_DIM, "Kein Touchpad vorhanden.");
    }

    /* Akku */
    char buf[64];
    fill(16, 514, 620, 246, C_PANEL);
    text(30, 524, 2, C_ACCENT, "Akku / Verbindung");
    text(30, 556, 2, C_TEXT, power_text(g, buf, sizeof buf));
    SDL_JoystickConnectionState cs = SDL_GetGamepadConnectionState(g);
    text(30, 580, 2, C_TEXT, cs == SDL_JOYSTICK_CONNECTION_WIRED ? "Kabel (USB)" : cs == SDL_JOYSTICK_CONNECTION_WIRELESS ? "kabellos" : "Verbindung unbekannt");

    /* LED */
    float x = 660, y = 114;
    fill(648, 104, 616, 656, C_PANEL);
    bool rgb = cap(g, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN);
    text(x, y, 2, C_ACCENT, rgb ? "Leuchtfarbe (RGB-LED)" : "Leuchtfarbe: keine RGB-LED");
    y += 34;
    if (rgb) {
        static const struct { const char *n; SDL_Color c; } cols[] = {
            {"Rot", {255, 0, 0, 255}}, {"Grün", {0, 255, 0, 255}}, {"Blau", {0, 0, 255, 255}},
            {"Weiß", {255, 255, 255, 255}}, {"Gelb", {255, 200, 0, 255}}, {"Lila", {180, 0, 255, 255}},
            {"Aus", {0, 0, 0, 255}},
        };
        for (int i = 0; i < 7; i++) {
            float bx = x + (i % 4) * 148, by = y + (i / 4) * 46;
            if (ui_button(bx, by, 144, 38, cols[i].n, false)) {
                led_rainbow = false;
                led_color = cols[i].c;
                if (!SDL_SetGamepadLED(g, led_color.r, led_color.g, led_color.b)) {
                    logmsg("LED: %s", SDL_GetError());
                }
            }
            fill(bx + 4, by + 32, 136, 3, cols[i].c);
        }
        if (ui_button(x + 3 * 148, y + 46, 144, 38, "Bunt", led_rainbow)) {
            led_rainbow = !led_rainbow;
        }
        y += 104;
        fill(x, y, 592, 20, led_color);
        y += 40;
    }

    bool pled = cap(g, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN);
    text(x, y, 2, C_ACCENT, pled ? "Spieler-LED" : "Spieler-LED: nicht gemeldet");
    y += 34;
    int pi = SDL_GetGamepadPlayerIndex(g);
    for (int i = 0; i < 4; i++) {
        char l[16];
        snprintf(l, sizeof l, "%d", i + 1);
        if (ui_button(x + i * 148, y, 144, 38, l, pi == i)) {
            SDL_SetGamepadPlayerIndex(g, i);
        }
    }
    y += 70;
    text(x, y, 2, C_DIM, "Zusatztasten: siehe Tab Tasten.");
}

/* ------------------------------------------------------------------ */
/* Rahmen                                                              */
/* ------------------------------------------------------------------ */

static void draw_no_pad(void)
{
    text_center(640, 300, 3, C_TEXT, "Kein Controller verbunden");
    text_center(640, 360, 2, C_DIM, "Schließe jetzt einen Controller per USB an");
    text_center(640, 384, 2, C_DIM, "oder verbinde ihn per Bluetooth.");
    text_center(640, 430, 2, C_DIM, "Switch 2 Pro Controller: nur per USB-Kabel.");
    if (logcount) {
        text_center(640, 500, 2, C_ACCENT, "Ereignisse");
        draw_log(440, 530, 6, 40);
    }
}

/* Für Tests: GAMEPAD_TESTER_SHOT=datei.bmp speichert nach einigen Frames ein Bild und beendet */
static const char *shot_path;
static int shot_frames;
static bool shot_done;

static void save_screenshot(void)
{
    if (!shot_path || ++shot_frames < 30) {
        return;
    }
    SDL_Surface *s = SDL_RenderReadPixels(ren, NULL);
    if (s) {
        SDL_SaveBMP(s, shot_path);
        SDL_DestroySurface(s);
    }
    shot_done = true;
}

static void draw(void)
{
    set_color(C_BG);
    SDL_RenderClear(ren);

    SDL_Gamepad *g = cur();
    text(16, 14, 3, C_TEXT, "Gamepad-Tester");
    if (g) {
        char buf[96];
        snprintf(buf, sizeof buf, "%.40s", SDL_GetGamepadName(g));
        float w = text_width(2, buf);
        text(1264 - w, 10, 2, C_ACCENT, buf);
        snprintf(buf, sizeof buf, "Controller %d von %d%s", act + 1, npads, npads > 1 ? "  [N] wechseln" : "");
        w = text_width(2, buf);
        text(1264 - w, 32, 2, C_DIM, buf);
    }

    for (int i = 0; i < TAB_COUNT; i++) {
        float x = 10 + i * 181;
        if (ui_button(x, 58, 175, 36, tab_names[i], i == tab)) {
            tab = i;
        }
        textf(x + 4, 61, 1, i == tab ? C_BG : C_DIM, "%d", i + 1);
    }

    if (!g) {
        draw_no_pad();
    } else {
        switch (tab) {
        case TAB_INFO: tab_info(g); break;
        case TAB_BUTTONS: tab_buttons(g); break;
        case TAB_STICKS: tab_sticks(g); break;
        case TAB_DRIFT: tab_drift(); break;
        case TAB_RUMBLE: tab_rumble(g); break;
        case TAB_GYRO: tab_gyro(g); break;
        case TAB_EXTRAS: tab_extras(g); break;
        }
    }

    fill(0, H - 30, W, 30, C_PANEL);
    text(16, H - 23, 2, C_DIM, "1-7 / Pfeiltasten: Tab   N: Controller   R: zurücksetzen   Esc: Beenden");
    save_screenshot();
    SDL_RenderPresent(ren);
}

static void on_reset_key(void)
{
    switch (tab) {
    case TAB_BUTTONS: reset_buttons(); logmsg("Checkliste zurückgesetzt"); break;
    case TAB_STICKS: memset(trail, 0, sizeof trail); break;
    case TAB_DRIFT: rt_state = RT_IDLE; memset(ct_run, 0, sizeof ct_run); memset(ct_bin, 0, sizeof ct_bin); break;
    case TAB_RUMBLE: start_rumble(RM_NONE); break;
    case TAB_GYRO: reset_gyro(); break;
    case TAB_EXTRAS: memset(touch_trail, 0, sizeof touch_trail); break;
    default: break;
    }
}

/* Für Tests ohne Hardware: GAMEPAD_TESTER_VIRTUAL=1 hängt einen virtuellen Controller an */
static bool SDLCALL virt_rumble(void *ud, Uint16 lo, Uint16 hi)
{
    (void)ud;
    (void)lo;
    (void)hi;
    return true;
}

static bool SDLCALL virt_led(void *ud, Uint8 r, Uint8 g, Uint8 b)
{
    (void)ud;
    (void)r;
    (void)g;
    (void)b;
    return true;
}

static void attach_virtual_pad(void)
{
    static SDL_VirtualJoystickTouchpadDesc tp = {2, {0, 0, 0}};
    static SDL_VirtualJoystickSensorDesc sensors[2] = {{SDL_SENSOR_GYRO, 250}, {SDL_SENSOR_ACCEL, 250}};
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.ntouchpads = 1;
    d.touchpads = &tp;
    d.nsensors = 2;
    d.sensors = sensors;
    d.name = "Virtueller Test-Controller";
    d.Rumble = virt_rumble;
    d.SetLED = virt_led;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
    if (!id) {
        SDL_Log("Virtueller Controller fehlgeschlagen: %s", SDL_GetError());
    }
}

static void poke_virtual_pad(void)
{
    SDL_Gamepad *g = cur();
    if (!g || !SDL_IsJoystickVirtual(SDL_GetGamepadID(g))) {
        return;
    }
    SDL_Joystick *j = SDL_GetGamepadJoystick(g);
    float t = (float)SDL_GetTicks() / 1000.0f;
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, (Sint16)(cosf(t * 3) * 32000));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, (Sint16)(sinf(t * 3) * 32000));
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTX, 900);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTY, -1400);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 20000);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_SOUTH, true);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    SDL_SetJoystickVirtualTouchpad(j, 0, 0, true, 0.3f, 0.4f, 1.0f);
    float gyr[3] = {0.4f * sinf(t * 2), 0.6f, 0.2f};
    float acc[3] = {0.5f, 9.7f, -0.8f};
    Uint64 ns = SDL_GetTicksNS();
    SDL_SendJoystickVirtualSensorData(j, SDL_SENSOR_GYRO, ns, gyr, 3);
    SDL_SendJoystickVirtualSensorData(j, SDL_SENSOR_ACCEL, ns, acc, 3);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    /* Gamepads auch dann lesen, wenn das Fenster nicht im Vordergrund ist */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    /* DualSense/DS4: erweiterte Berichte für Gyro, Touchpad, Vibration und LED */
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
    /* Switch-2-Controller laufen über libusb */
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_SWITCH2", "1");
    SDL_SetHint(SDL_HINT_HIDAPI_LIBUSB, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init fehlgeschlagen: %s", SDL_GetError());
        return 1;
    }
    if (!SDL_CreateWindowAndRenderer("Gamepad-Tester", W, H, SDL_WINDOW_RESIZABLE, &win, &ren)) {
        SDL_Log("Fenster fehlgeschlagen: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderLogicalPresentation(ren, W, H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(ren, 1);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    logmsg("Programm gestartet");
    shot_path = SDL_getenv("GAMEPAD_TESTER_SHOT");
    bool virt = SDL_getenv("GAMEPAD_TESTER_VIRTUAL") != NULL;
    if (virt) {
        attach_virtual_pad();
    }
    if (SDL_getenv("GAMEPAD_TESTER_TAB")) {
        tab = SDL_clamp(SDL_atoi(SDL_getenv("GAMEPAD_TESTER_TAB")), 0, TAB_COUNT - 1);
    }

    bool running = true;
    while (running && !shot_done) {
        mclick = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL_ConvertEventToRenderCoordinates(ren, &e);
            SDL_Gamepad *g = cur();
            SDL_JoystickID gid = g ? SDL_GetGamepadID(g) : 0;
            switch (e.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (e.key.key >= SDLK_1 && e.key.key < SDLK_1 + TAB_COUNT) {
                    tab = (int)(e.key.key - SDLK_1);
                } else if (e.key.key == SDLK_RIGHT) {
                    tab = (tab + 1) % TAB_COUNT;
                } else if (e.key.key == SDLK_LEFT) {
                    tab = (tab + TAB_COUNT - 1) % TAB_COUNT;
                } else if (e.key.key == SDLK_N && npads > 1) {
                    select_pad((act + 1) % npads);
                } else if (e.key.key == SDLK_R) {
                    on_reset_key();
                }
                break;
            case SDL_EVENT_MOUSE_MOTION: mx = e.motion.x; my = e.motion.y; break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mx = e.button.x;
                    my = e.button.y;
                    mdown = mclick = true;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mdown = false;
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED: add_pad(e.gdevice.which); break;
            case SDL_EVENT_GAMEPAD_REMOVED: remove_pad(e.gdevice.which); break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (g && e.gbutton.which == gid && e.gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
                    seen[e.gbutton.button] = true;
                    logmsg("%s gedrückt", btn_name(g, (SDL_GamepadButton)e.gbutton.button));
                }
                break;
            case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
                if (g && e.gsensor.which == gid) {
                    on_sensor((SDL_SensorType)e.gsensor.sensor, e.gsensor.data, e.gsensor.sensor_timestamp);
                }
                break;
            default: break;
            }
        }
        if (virt) {
            poke_virtual_pad();
        }
        update();
        draw();
    }

    stop_rumble(cur());
    for (int i = 0; i < npads; i++) {
        SDL_CloseGamepad(pads[i]);
    }
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
