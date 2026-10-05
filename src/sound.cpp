// Procedural sound: everything is synthesised in the miniaudio callback, no samples.

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include "sound.h"

#include <atomic>
#include <chrono>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace sfx {

namespace {

constexpr float TAU = 6.2831853f;
constexpr int   SR  = 48000;
constexpr float DT  = 1.f / SR;

// ---- lock-free SPSC queue: main thread -> audio thread ----
struct Msg { Event e; float x, p; int ship; };
constexpr unsigned QN = 128;
Msg q[QN];
std::atomic<unsigned> qHead{0}, qTail{0};

struct EngineCtl { std::atomic<bool> thrust{false}; std::atomic<int> rot{0}; std::atomic<float> x{0.f}; };
EngineCtl ctl[2];
std::atomic<float> shipGain[2]{{1.f}, {1.f}};

// ---- audio-thread state ----
uint32_t rng = 0x9E3779B9u;
inline float noise() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (int32_t)rng * (1.f / 2147483648.f); }
inline float lpCoef(float fc) { return 1.f - expf(-TAU * fc * DT); }
inline float sq(float ph) { return tanhf(4.f * sinf(TAU * ph)); }   // soft square

struct Voice {
    bool active = false;
    Event e; float t, dur, pan, p, gain; int ship;
    float ph[4], lp[3], crackle;
};
constexpr int NV = 32;
Voice voices[NV];

struct EngineState {
    float thrustLvl = 0, rotLvl = 0, rotDir = 0, pan = 0;
    float humPh = 0, humLp = 0, n1 = 0, n2 = 0, flutter = 0, hp = 0, hpLp = 0, puffPh = 0, sp = 0;
};
EngineState eng[2];

const float kDur[NumEvents] = {
    0.40f, 0.45f, 0.90f, 1.30f, 0.30f, 0.80f, 0.70f, 0.10f, 4.0f, 2.4f, 0.65f, 0.14f, 1.1f,
    0.12f, 0.05f, 0.35f, 0.08f
};

void spawn(const Msg& m) {
    Voice* v = nullptr;
    for (auto& c : voices) if (!c.active) { v = &c; break; }
    if (!v) {   // steal the oldest
        v = &voices[0];
        for (auto& c : voices) if (c.t > v->t) v = &c;
    }
    *v = Voice{};
    v->active = true; v->e = m.e; v->t = 0; v->dur = kDur[m.e];
    v->pan = fmaxf(-1.f, fminf(1.f, m.x / 1.75f)); v->p = m.p;
    v->ship = m.ship < 0 ? 0 : m.ship;
    v->gain = m.ship < 0 ? 1.f : shipGain[m.ship].load(std::memory_order_relaxed);
    v->ph[0] = v->ph[1] = v->ph[2] = v->ph[3] = 0; v->lp[0] = v->lp[1] = v->lp[2] = 0; v->crackle = 0;
}

inline float bell(float f, float tau, float decay) {
    if (tau < 0) return 0;
    return (sinf(TAU * f * tau) + 0.35f * sinf(TAU * 2.76f * f * tau) + 0.12f * sinf(TAU * 5.4f * f * tau)) * expf(-tau * decay);
}

inline float metal(float f0, float t, float decayScale) {
    static const float r[4] = {1.f, 2.76f, 5.40f, 8.93f};
    static const float d[4] = {6.f, 9.f, 14.f, 20.f};
    float s = 0;
    for (int k = 0; k < 4; ++k) s += sinf(TAU * f0 * r[k] * t) * expf(-t * d[k] * decayScale) / (k + 1);
    return s;
}

inline float brass(float f, float tau, float len) {
    if (tau < 0 || tau > len + 0.25f) return 0;
    float env = fminf(1.f, tau / 0.02f) * (tau < len ? 1.f : expf(-(tau - len) * 18.f));
    float vib = 1.f + 0.006f * sinf(TAU * 5.5f * tau) * fminf(1.f, tau / 0.3f);
    float s = 0;
    for (int k = 1; k <= 7; ++k) s += sinf(TAU * k * f * vib * tau) / k;
    return s * env;
}

float render(Voice& v) {
    const float t = v.t;
    float s = 0;
    switch (v.e) {
    case Thud: {                       // pitch-dropping body + dirt
        float f = 38.f + 120.f * expf(-t * 22.f);
        v.ph[0] += f * DT;
        v.lp[0] += lpCoef(900.f) * (noise() - v.lp[0]);
        s = (sinf(TAU * v.ph[0]) * expf(-t * 10.f) + 1.4f * v.lp[0] * expf(-t * 45.f)) * (0.25f + 0.75f * v.p) * 0.9f;
        break;
    }
    case Clank:
        s = metal(310.f + 40.f * v.ship, t, 1.4f) * 0.35f + noise() * expf(-t * 90.f) * 0.3f;
        break;
    case Clang: {                      // two hulls ringing against each other
        float a = metal(173.f, t, 0.6f), b = metal(229.f, t, 0.7f);
        s = (a + b) * 0.28f * (1.f + 0.4f * sinf(TAU * 31.f * t)) + noise() * expf(-t * 60.f) * 0.5f;
        break;
    }
    case Chime: {                      // touchdown: rising bell arpeggio
        static const float f[4] = {1046.5f, 1318.5f, 1568.0f, 2093.0f};
        float tr = v.ship == 1 ? 1.1225f : 1.f;
        for (int k = 0; k < 4; ++k) s += bell(f[k] * tr, t - 0.075f * k, 4.5f);
        s *= 0.16f;
        break;
    }
    case Collect: {                    // zip upward
        float f = 500.f * powf(4.f, fminf(1.f, t / 0.12f));
        v.ph[0] += f * DT; v.ph[1] += f * 1.5f * DT;
        s = (sq(v.ph[0]) * 0.6f + sinf(TAU * v.ph[1]) * 0.4f) * expf(-t * 9.f) * 0.22f;
        break;
    }
    case Score: {
        static const float f[4] = {783.99f, 987.77f, 1174.66f, 1567.98f};
        float tr = v.ship == 1 ? 1.1225f : 1.f;
        for (int k = 0; k < 4; ++k) {
            float tau = t - 0.07f * k;
            if (tau >= 0) s += sq(f[k] * tr * tau) * expf(-tau * (k == 3 ? 3.5f : 9.f));
        }
        s *= 0.13f;
        break;
    }
    case Boing: {                      // spring: wobbling pitch, decaying
        float f = 180.f * (1.f + 0.45f * sinf(TAU * 11.f * t) * expf(-t * 4.f)) * (1.f + 0.6f * expf(-t * 8.f));
        v.ph[0] += f * DT;
        s = (sinf(TAU * v.ph[0]) + 0.3f * sinf(TAU * 2.f * v.ph[0])) * expf(-t * 5.f) * 0.35f;
        break;
    }
    case Refuel: {                     // bubble, pitch climbs as the tank fills
        float f = (350.f + 700.f * v.p) * (1.f + 6.f * t);
        v.ph[0] += f * DT;
        s = sinf(TAU * v.ph[0]) * fminf(1.f, t / 0.004f) * expf(-t * 40.f) * 0.28f;
        break;
    }
    case Explosion: {
        float fc = 60.f + 5000.f * expf(-t * 2.2f);
        float c = lpCoef(fc);
        float n = noise();
        v.lp[0] += c * (n - v.lp[0]);
        v.lp[1] += c * (v.lp[0] - v.lp[1]);
        if (noise() > 1.f - 0.004f * expf(-t * 0.8f)) v.crackle = 0.6f + 0.4f * fabsf(noise());
        v.crackle *= 0.993f;
        float fs = 22.f + 70.f * expf(-t * 1.6f);
        v.ph[0] += fs * DT;
        float env = fminf(1.f, t / 0.004f) * expf(-t * 0.9f);
        s = v.lp[1] * 2.2f * env
          + sinf(TAU * v.ph[0]) * expf(-t * 1.3f) * 0.8f
          + noise() * v.crackle * 0.35f * expf(-t * 0.5f);
        break;
    }
    case Win: {                        // brass fanfare
        static const float on[5]  = {0.00f, 0.16f, 0.32f, 0.48f, 0.80f};
        static const float len[5] = {0.12f, 0.12f, 0.12f, 0.28f, 1.20f};
        static const float f[5]   = {523.25f, 659.25f, 783.99f, 1046.5f, 1046.5f};
        for (int k = 0; k < 5; ++k) s += brass(f[k], t - on[k], len[k]);
        s += 0.5f * brass(783.99f, t - 0.80f, 1.2f) + 0.4f * brass(659.25f, t - 0.80f, 1.2f);
        s *= 0.07f;
        break;
    }
    case Alarm: {
        int seg = (int)(t / 0.14f);
        if (seg < 4) {
            float tau = t - seg * 0.14f;
            float f = (seg & 1) ? 660.f : 880.f;
            s = sq(f * tau) * fminf(1.f, tau / 0.005f) * (tau < 0.11f ? 1.f : 0.f) * 0.10f;
        }
        break;
    }
    case Sputter: {                    // dry cough from an empty tank
        v.lp[0] += lpCoef(1200.f) * (noise() - v.lp[0]);
        v.ph[0] += (55.f + 25.f * v.p) * DT;
        s = (v.lp[0] * 1.5f + sinf(TAU * v.ph[0]) * 0.6f) * expf(-t * 28.f) * 0.5f;
        break;
    }
    case Start: {                      // systems powering up
        float f = 90.f * powf(8.f, fminf(1.f, t / 0.8f));
        v.ph[0] += f * DT; v.ph[1] += f * 1.005f * DT;
        float saw = (2.f * (v.ph[0] - floorf(v.ph[0])) - 1.f) + (2.f * (v.ph[1] - floorf(v.ph[1])) - 1.f);
        v.lp[0] += lpCoef(300.f + 3000.f * t) * (saw - v.lp[0]);
        s = v.lp[0] * fminf(1.f, t / 0.05f) * (t < 0.85f ? 1.f : expf(-(t - 0.85f) * 20.f)) * 0.12f;
        break;
    }
    case Fire: {                       // pneumatic pop: noise burst + falling blip
        float f = (v.ship == 1 ? 1500.f : 1200.f) * expf(-t * 30.f) + 200.f;
        v.ph[0] += f * DT;
        v.lp[0] += lpCoef(3500.f) * (noise() - v.lp[0]);
        s = (v.lp[0] * 1.6f * expf(-t * 70.f) + sinf(TAU * v.ph[0]) * expf(-t * 35.f) * 0.5f) * 0.35f;
        break;
    }
    case Dry:                          // empty click
        s = noise() * expf(-t * 400.f) * 0.3f + sinf(TAU * 2400.f * t) * expf(-t * 250.f) * 0.15f;
        break;
    case Ping:                         // pellet rings the hull
        s = metal(820.f + 110.f * v.ship, t, 1.8f) * 0.3f + noise() * expf(-t * 200.f) * 0.2f;
        break;
    case Reload: {                     // ratchet tick, pitch climbs as magazine fills
        float f = 900.f + 900.f * v.p;
        s = (sinf(TAU * f * t) * expf(-t * 60.f) + noise() * expf(-t * 300.f) * 0.5f) * 0.2f;
        break;
    }
    default: break;
    }
    v.t += DT;
    if (v.t >= v.dur) v.active = false;
    return s;
}

float renderEngine(int i, float& pan) {
    EngineState& e = eng[i];
    bool thrust = ctl[i].thrust.load(std::memory_order_relaxed);
    int rot = ctl[i].rot.load(std::memory_order_relaxed);
    float x = ctl[i].x.load(std::memory_order_relaxed);

    e.thrustLvl += (thrust ? 0.0015f : 0.00025f) * ((thrust ? 1.f : 0.f) - e.thrustLvl);
    e.rotLvl    += (rot ? 0.004f : 0.0008f) * ((rot ? 1.f : 0.f) - e.rotLvl);
    if (rot) e.rotDir = (float)rot;
    e.pan += 0.0005f * (fmaxf(-1.f, fminf(1.f, x / 1.75f)) - e.pan);
    pan = e.pan;

    float s = 0;
    if (e.thrustLvl > 1e-4f) {
        // rocket: brown-ish rumble with flutter, plus a gritty low hum per ship
        e.flutter += 0.002f * (noise() - e.flutter);
        float n = noise();
        e.n1 += lpCoef(180.f + 700.f * e.thrustLvl) * (n - e.n1);
        e.n2 += lpCoef(90.f + 350.f * e.thrustLvl) * (e.n1 - e.n2);
        float f = (i == 0 ? 52.f : 69.f) * (1.f + 0.25f * e.thrustLvl + 3.f * e.flutter);
        e.humPh += f * DT; e.humPh -= floorf(e.humPh);
        e.humLp += lpCoef(260.f) * ((2.f * e.humPh - 1.f) - e.humLp);
        float grit = 1.f + 0.7f * e.flutter * 20.f;
        s += (e.n2 * 5.0f * grit + e.humLp * 0.35f + e.n1 * 0.35f) * e.thrustLvl * 0.55f;
    }
    if (e.rotLvl > 1e-4f) {
        // RCS: puffed high-passed hiss; CW and CCW puff at different rates and colours
        float n = noise();
        e.hpLp += lpCoef(e.rotDir > 0 ? 2600.f : 1700.f) * (n - e.hpLp);
        float hp = n - e.hpLp;
        e.hp += lpCoef(7000.f) * (hp - e.hp);
        e.puffPh += (e.rotDir > 0 ? 19.f : 14.f) * DT; e.puffPh -= floorf(e.puffPh);
        float puff = 0.35f + 0.65f * expf(-e.puffPh * 9.f);
        s += e.hp * puff * e.rotLvl * 0.22f;
    }
    return s;
}

void callback(ma_device*, void* out, const void*, ma_uint32 frames) {
    unsigned h = qHead.load(std::memory_order_acquire);
    unsigned tl = qTail.load(std::memory_order_relaxed);
    while (tl != h) { spawn(q[tl % QN]); ++tl; }
    qTail.store(tl, std::memory_order_release);

    const float engGain[2] = {shipGain[0].load(std::memory_order_relaxed),
                              shipGain[1].load(std::memory_order_relaxed)};
    float* o = static_cast<float*>(out);
    for (ma_uint32 n = 0; n < frames; ++n) {
        float L = 0, R = 0;
        for (auto& v : voices) {
            if (!v.active) continue;
            float s = render(v) * v.gain;
            float a = (v.pan + 1.f) * 0.25f * TAU * 0.5f;   // equal-power pan
            L += s * cosf(a); R += s * sinf(a);
        }
        for (int i = 0; i < 2; ++i) {
            float pan;
            float s = renderEngine(i, pan) * engGain[i];
            float a = (pan + 1.f) * 0.25f * TAU * 0.5f;
            L += s * cosf(a); R += s * sinf(a);
        }
        o[2 * n]     = tanhf(L * 0.9f);
        o[2 * n + 1] = tanhf(R * 0.9f);
    }
}

ma_device device;
bool ok = false;

const float kCooldown[NumEvents] = {
    0.09f, 0.15f, 0.35f, 0.f, 0.f, 0.f, 0.3f, 0.f, 0.f, 0.f, 0.f, 0.16f, 0.f,
    0.f, 0.15f, 0.05f, 0.f
};
double lastPlay[NumEvents][3];

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

bool init() {
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format   = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate        = SR;
    cfg.dataCallback      = callback;
    if (ma_device_init(nullptr, &cfg, &device) != MA_SUCCESS) {
        fprintf(stderr, "sound: no playback device, running silent\n");
        return false;
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        fprintf(stderr, "sound: failed to start device, running silent\n");
        return false;
    }
    ok = true;
    return true;
}

void shutdown() {
    if (ok) ma_device_uninit(&device);
    ok = false;
}

void play(Event e, float x, float param, int ship) {
    if (!ok) return;
    double t = now();
    double& last = lastPlay[e][ship + 1];
    if (t - last < kCooldown[e]) return;
    last = t;

    unsigned h = qHead.load(std::memory_order_relaxed);
    if (h - qTail.load(std::memory_order_acquire) >= QN) return;   // full: drop
    q[h % QN] = Msg{e, x, param, ship};
    qHead.store(h + 1, std::memory_order_release);
}

void engine(int ship, bool thrust, int rot, float x) {
    ctl[ship].thrust.store(thrust, std::memory_order_relaxed);
    ctl[ship].rot.store(rot, std::memory_order_relaxed);
    ctl[ship].x.store(x, std::memory_order_relaxed);
}

void setShipVolume(int ship, float gain) {
    if (ship < 0 || ship > 1) return;
    shipGain[ship].store(gain, std::memory_order_relaxed);
}

}
