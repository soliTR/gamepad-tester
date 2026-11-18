/* Quaternions for the orientation of the controller in the 3D view */
#ifndef QUAT_H
#define QUAT_H

#include <math.h>

typedef struct {
    float w, x, y, z;
} Quat;

static const Quat QUAT_IDENTITY = {1, 0, 0, 0};

static inline Quat quat_mul(Quat a, Quat b)
{
    return (Quat){a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                  a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                  a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                  a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

/* Rotates the vector v by q */
static inline void quat_rotate(Quat q, const float v[3], float out[3])
{
    Quat p = {0, v[0], v[1], v[2]};
    Quat conj = {q.w, -q.x, -q.y, -q.z};
    Quat r = quat_mul(quat_mul(q, p), conj);
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

/* Rotates q by the angular velocity w (rad/s, controller coordinates) for dt seconds */
static inline Quat quat_integrate(Quat q, const float w[3], float dt)
{
    float n = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    if (n < 1e-9f) {
        return q;
    }
    float half = n * dt / 2, s = sinf(half) / n;
    Quat d = {cosf(half), w[0] * s, w[1] * s, w[2] * s};
    Quat r = quat_mul(q, d);
    float len = sqrtf(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
    return (Quat){r.w / len, r.x / len, r.y / len, r.z / len};
}

#endif
