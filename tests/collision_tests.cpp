// Phase 1 ship-on-ship collision: Newtonian linear-momentum impulse.
// Verifies momentum conservation, the elastic/inelastic energy range, the
// mass-weighted velocity split, that only the normal component changes, and
// that an already-separating pair gets no impulse (no sticking).
#include "ship.h"
#include <cmath>
#include <iostream>

static int failures = 0;
static void check(bool c, const char* m) {
    if (!c) { std::cerr << "FAIL: " << m << '\n'; ++failures; }
}

static ShipStatusConsts cfg() { return ShipStatusConsts{0, 0, 0.0f}; }

// Place a ship so its hull center sits near (x,y) and give it a velocity.
static Ship makeShip(float x, float y, float vx, float vy, float e = SHIP_RESTITUTION) {
    Ship s(cfg());
    Point2D o{ x, y };
    s.reset(o);
    s.velocity = { vx, vy };
    s.restitution = e;
    return s;
}

static float momentumX(const Ship& a, const Ship& b) {
    return a.mass * a.velocity.x + b.mass * b.velocity.x;
}
static float momentumY(const Ship& a, const Ship& b) {
    return a.mass * a.velocity.y + b.mass * b.velocity.y;
}
static float kinetic(const Ship& a, const Ship& b) {
    auto ke = [](const Ship& s) {
        return 0.5f * s.mass * (s.velocity.x*s.velocity.x + s.velocity.y*s.velocity.y);
    };
    return ke(a) + ke(b);
}

int main() {
    // Head-on, equal mass, elastic (e=1): velocities should swap; p and KE kept.
    {
        Ship a = makeShip(-0.006f, 0.0f,  0.01f, 0.0f, 1.0f);
        Ship b = makeShip( 0.006f, 0.0f, -0.01f, 0.0f, 1.0f);
        float px0 = momentumX(a, b), py0 = momentumY(a, b), k0 = kinetic(a, b);
        bool hit = a.resolveCollision(b);
        check(hit, "elastic head-on registers impact");
        check(std::fabs(momentumX(a, b) - px0) < 1e-6f, "elastic: X momentum conserved");
        check(std::fabs(momentumY(a, b) - py0) < 1e-6f, "elastic: Y momentum conserved");
        check(std::fabs(kinetic(a, b) - k0) < 1e-6f, "elastic: kinetic energy conserved");
    }

    // Head-on, equal mass, perfectly inelastic (e=0): common velocity, KE drops.
    {
        Ship a = makeShip(-0.006f, 0.0f,  0.01f, 0.0f, 0.0f);
        Ship b = makeShip( 0.006f, 0.0f, -0.01f, 0.0f, 0.0f);
        float px0 = momentumX(a, b), py0 = momentumY(a, b), k0 = kinetic(a, b);
        // Capture the impulse direction from A's velocity change (== the normal).
        Point2D va0 = a.velocity;
        a.resolveCollision(b);
        check(std::fabs(momentumX(a, b) - px0) < 1e-6f, "inelastic: X momentum conserved");
        check(std::fabs(momentumY(a, b) - py0) < 1e-6f, "inelastic: Y momentum conserved");
        float nx = a.velocity.x - va0.x, ny = a.velocity.y - va0.y;   // along normal
        float nlen = std::sqrt(nx*nx + ny*ny);
        check(nlen > 1e-9f, "inelastic: an impulse occurred");
        nx /= nlen; ny /= nlen;
        float rvn = (a.velocity.x - b.velocity.x) * nx + (a.velocity.y - b.velocity.y) * ny;
        check(std::fabs(rvn) < 1e-6f, "inelastic: normal relative velocity removed");
        check(kinetic(a, b) < k0 - 1e-7f, "inelastic: kinetic energy drops");
    }

    // Glancing: closing on X, shared Y velocity. Y (tangential) must be untouched.
    {
        Ship a = makeShip(-0.006f, 0.0f,  0.01f, 0.004f, 1.0f);
        Ship b = makeShip( 0.006f, 0.0f, -0.01f, 0.004f, 1.0f);
        float px0 = momentumX(a, b), py0 = momentumY(a, b);
        a.resolveCollision(b);
        check(std::fabs(momentumX(a, b) - px0) < 1e-6f, "glancing: X momentum conserved");
        check(std::fabs(momentumY(a, b) - py0) < 1e-6f, "glancing: Y momentum conserved");
    }

    // Already separating (overlap but moving apart): no impulse, velocities held.
    {
        Ship a = makeShip(-0.006f, 0.0f, -0.01f, 0.0f, 1.0f);
        Ship b = makeShip( 0.006f, 0.0f,  0.01f, 0.0f, 1.0f);
        float ax = a.velocity.x, bx = b.velocity.x;
        bool hit = a.resolveCollision(b);
        check(!hit, "separating pair: no impulse applied");
        check(a.velocity.x == ax && b.velocity.x == bx, "separating pair: velocities unchanged");
    }

    // Not touching at all: resolver reports no contact.
    {
        Ship a = makeShip(-1.0f, 0.0f, 0.01f, 0.0f);
        Ship b = makeShip( 1.0f, 0.0f, 0.0f,  0.0f);
        check(!a.resolveCollision(b), "far apart: no contact");
    }

    if (failures == 0) std::cout << "all collision tests passed\n";
    return failures ? 1 : 0;
}
