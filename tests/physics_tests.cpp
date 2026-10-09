// PhysicsBody base-layer tests: the shared Newtonian core underneath every
// collidable object (ships, balls, pellets). Verifies the constructor, the
// impulse law (momentum conservation; energy conserved at e=1 and lost as e
// drops), mass-weighted velocity split, the separating-pair no-op, and the
// static surface bounce (dull ground vs. lively pad).
#include "physics.h"
#include <cmath>
#include <iostream>

static int failures = 0;
static void check(bool c, const char* m) {
    if (!c) { std::cerr << "FAIL: " << m << '\n'; ++failures; }
}
static bool near(float a, float b, float eps = 1e-4f) { return fabsf(a - b) < eps; }

static float momentumX(const PhysicsBody& a, const PhysicsBody& b) {
    return a.mass * a.velocity.x + b.mass * b.velocity.x;
}
static float kinetic(const PhysicsBody& a, const PhysicsBody& b) {
    auto ke = [](const PhysicsBody& s) {
        return 0.5f * s.mass * (s.velocity.x*s.velocity.x + s.velocity.y*s.velocity.y);
    };
    return ke(a) + ke(b);
}

int main() {
    // --- Constructor sets every field --------------------------------------
    {
        PhysicsBody b(2.0f, {1.0f, 2.0f}, {3.0f, 4.0f}, 0.5f);
        check(near(b.mass, 2.0f),         "ctor mass");
        check(near(b.pos.x, 1.0f) && near(b.pos.y, 2.0f),       "ctor pos");
        check(near(b.velocity.x, 3.0f) && near(b.velocity.y, 4.0f), "ctor velocity");
        check(near(b.restitution, 0.5f), "ctor restitution");
        check(near(b.invMass(), 0.5f),   "invMass = 1/m");
    }

    // --- Head-on, equal mass, e=1: velocities exchange, energy conserved ---
    {
        PhysicsBody a(1.0f, {0,0}, { 1.0f, 0}, 1.0f);
        PhysicsBody c(1.0f, {0,0}, {-1.0f, 0}, 1.0f);
        float p0 = momentumX(a, c), k0 = kinetic(a, c);
        bool hit = a.resolveImpulse(c, {-1.0f, 0.0f});  // normal points from c back toward a
        check(hit, "elastic pair collides");
        check(near(momentumX(a, c), p0), "momentum conserved (elastic)");
        check(near(kinetic(a, c), k0),   "kinetic energy conserved at e=1");
    }

    // --- Inelastic (e=0): momentum conserved, energy LOST -------------------
    {
        PhysicsBody a(1.0f, {0,0}, { 1.0f, 0}, 0.0f);
        PhysicsBody c(1.0f, {0,0}, {-1.0f, 0}, 0.0f);
        float p0 = momentumX(a, c), k0 = kinetic(a, c);
        a.resolveImpulse(c, {-1.0f, 0.0f});
        check(near(momentumX(a, c), p0), "momentum conserved (inelastic)");
        check(kinetic(a, c) < k0 - 1e-4f, "kinetic energy lost at e=0");
    }

    // --- Mass ratio: light ball hits heavy ship, ship barely moves ---------
    {
        PhysicsBody ball(0.1f, {0,0}, { 2.0f, 0}, 0.6f);
        PhysicsBody ship(1.0f, {0,0}, { 0.0f, 0}, 0.6f);
        float p0 = momentumX(ball, ship);
        ball.resolveImpulse(ship, {-1.0f, 0.0f});
        check(near(momentumX(ball, ship), p0), "momentum conserved (ball->ship)");
        check(ship.velocity.x > 0.0f,          "ship gets pushed");
        check(ship.velocity.x < 0.5f,           "heavy ship moves little (<< ball's incoming 2.0)");
    }

    // --- Already separating: no impulse, no stick --------------------------
    {
        PhysicsBody a(1.0f, {0,0}, {-1.0f, 0}, 1.0f);
        PhysicsBody c(1.0f, {0,0}, { 1.0f, 0}, 1.0f);
        bool hit = a.resolveImpulse(c, {-1.0f, 0.0f});
        check(!hit, "separating pair gets no impulse");
    }

    // --- Static bounce: ground dull (e low) vs pad lively (e high) ----------
    {
        PhysicsBody dull(1.0f, {0,0}, {0.0f, -1.0f}, 0.0f);
        float v0 = -1.0f;
        dull.bounceOffStatic({0.0f, 1.0f}, 0.2f);   // ground e=0.2
        check(near(dull.velocity.y, 0.2f), "dull ground bounce keeps 20% speed");

        PhysicsBody lively(1.0f, {0,0}, {0.0f, -1.0f}, 0.0f);
        lively.bounceOffStatic({0.0f, 1.0f}, 0.9f); // pad e=0.9
        check(near(lively.velocity.y, 0.9f), "lively pad bounce keeps 90% speed");
        check(lively.velocity.y > dull.velocity.y,  "pad bounces higher than ground");
        (void)v0;
    }

    if (failures == 0) std::cout << "physics_tests: all passed\n";
    else               std::cerr << "physics_tests: " << failures << " FAILED\n";
    return failures ? 1 : 0;
}
