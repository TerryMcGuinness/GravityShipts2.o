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
// Total kinetic energy, translational + rotational (Phase 2: a nose-into-side
// contact is off-centre even when the ships close head-on, so some KE lives
// in spin; at e=1 the SUM is what is conserved).
static float kinetic(const Ship& a, const Ship& b) {
    auto ke = [](const Ship& s) {
        return 0.5f * s.mass * (s.velocity.x*s.velocity.x + s.velocity.y*s.velocity.y)
             + 0.5f * s.inertia * s.angVel * s.angVel;
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
        check(std::fabs(a.angVel) < SHIP_SPIN_MAX && std::fabs(b.angVel) < SHIP_SPIN_MAX, "elastic: spin stays under the cap (cap would sink energy)");
        check(std::fabs(kinetic(a, b) - k0) < 1e-6f, "elastic: kinetic energy (linear + rotational) conserved");
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
        // With spin, e=0 removes the closing speed AT THE CONTACT, not between
        // the centres; between the centres it must at least no longer be closing
        // as fast as before (was 0.02 head-on).
        float rvn = (a.velocity.x - b.velocity.x) * nx + (a.velocity.y - b.velocity.y) * ny;
        check(std::fabs(rvn) < 0.02f * 0.5f, "inelastic: centre closing speed mostly removed");
        check(a.angVel != 0.0f || b.angVel != 0.0f, "inelastic head-on: triangle contact is off-centre, spin induced");
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
    // --- Ship IS a PhysicsBody: static bounces use its restitution, not a sign flip ---
    {
        Ship s = makeShip(0.0f, 0.5f, 0.0f, -1.0f, 0.6f);
        PhysicsBody& body = s;                      // must upcast
        check(body.bounceOffStatic({0.0f, 1.0f}, s.restitution), "ship ground bounce fires");
        check(std::fabs(s.velocity.y - 0.6f) < 1e-6f, "ship ground bounce keeps e=0.6 of speed (was 100%)");
        check(!body.bounceOffStatic({0.0f, 1.0f}, s.restitution), "moving away: no second bounce");
    }

    // --- Ship integrates through PhysicsBody: gravity, landed, impulse/mass ---
    {
        Ship s = makeShip(0.0f, 0.5f, 0.0f, 0.0f);
        s.landed = false;
        s.updatePosition();
        check(std::fabs(s.velocity.y + GRAVITY) < 1e-9f, "updatePosition applies gravity via integrate()");
        check(std::fabs(s.spaceShip.offset.y - (0.5f - GRAVITY)) < 1e-6f, "offset synced back from pos");
        check(std::fabs(s.spaceShip.location[0].y - (s.spaceShip.vertices[0].y + s.spaceShip.offset.y)) < 1e-6f, "hull rebuilt on offset");

        Ship l = makeShip(0.0f, 0.5f, 0.0f, 0.0f);
        l.landed = true;
        l.updatePosition();
        check(l.velocity.y == 0.0f, "landed ship: no gravity");

        PhysicsBody light(1.0f, {0,0}, {0,0}, 1.0f), heavy(2.0f, {0,0}, {0,0}, 1.0f);
        light.applyImpulse({0.0f, 0.01f}); heavy.applyImpulse({0.0f, 0.01f});
        check(std::fabs(light.velocity.y - 2.0f * heavy.velocity.y) < 1e-9f, "same impulse: double mass -> half dv");
    }

    // --- Phase 2: off-centre impulse -> spin; centre hit -> none; L conserved ---
    {
        auto angMomentum = [](const PhysicsBody& b) { // about the origin
            return b.inertia * b.angVel + b.mass * (b.pos.x * b.velocity.y - b.pos.y * b.velocity.x);
        };
        // A moving +x hits B (at rest) with the contact 0.02 above B's centre.
        PhysicsBody a(1.0f, {-0.05f, 0.02f}, { 0.01f, 0.0f}, 1.0f), b(1.0f, {0.0f, 0.0f}, {0,0}, 1.0f);
        a.inertia = b.inertia = 0.003f;
        Point2D contact{ -0.025f, 0.02f };
        Point2D rA{ contact.x - a.pos.x, contact.y - a.pos.y }, rB{ contact.x - b.pos.x, contact.y - b.pos.y };
        float L0 = angMomentum(a) + angMomentum(b);
        float px0 = a.mass * a.velocity.x + b.mass * b.velocity.x;
        check(a.resolveImpulseAt(b, {-1.0f, 0.0f}, rA, rB), "off-centre hit resolves");
        check(std::fabs(b.angVel) > 1e-4f,  "off-centre hit puts spin on the target");
        check(std::fabs(a.angVel) < 1e-6f,  "contact in line with A's centre: A does not spin");
        check(std::fabs(angMomentum(a) + angMomentum(b) - L0) < 1e-6f, "angular momentum conserved");
        check(std::fabs(a.mass * a.velocity.x + b.mass * b.velocity.x - px0) < 1e-6f, "linear momentum conserved");

        PhysicsBody c(1.0f, {-0.05f, 0.0f}, { 0.01f, 0.0f}, 1.0f), d(1.0f, {0.0f, 0.0f}, {0,0}, 1.0f);
        c.inertia = d.inertia = 0.003f;
        c.resolveImpulseAt(d, {-1.0f, 0.0f}, {0.025f, 0.0f}, {-0.025f, 0.0f});
        check(c.angVel == 0.0f && d.angVel == 0.0f, "centre hit: pure shove, no spin");
        check(std::fabs(d.velocity.x - 0.01f) < 1e-6f, "centre hit, e=1, equal mass: full velocity exchange");
    }

    // --- Pellet hit: ship actually moves (normal sign regression) and off-centre spins ---
    {
        Ship s = makeShip(0.0f, 0.5f, 0.0f, 0.0f);
        s.landed = false;
        Pellet p; p.active = true;
        p.pos = { s.spaceShip.offset.x - 0.02f, s.spaceShip.offset.y + 0.015f };  // from the left, above centre
        p.velocity = { 0.01f, 0.0f };
        s.bump(p);
        check(s.velocity.x > 1e-4f, "pellet from the left shoves the ship right (was a no-op with the normal reversed)");
        check(std::fabs(s.angVel) > 1e-5f, "pellet above centre puts spin on the ship");
        Ship c = makeShip(0.0f, 0.5f, 0.0f, 0.0f); c.landed = false;
        Pellet q; q.active = true; q.pos = { c.spaceShip.offset.x - 0.02f, c.spaceShip.offset.y }; q.velocity = { 0.01f, 0.0f };
        c.bump(q);
        check(std::fabs(c.angVel) < 1e-7f, "pellet through the centre: shove, no spin");
        check(c.velocity.x > s.velocity.x, "centre hit shoves harder than off-centre (some impulse went to spin)");
    }

    return failures ? 1 : 0;
}
