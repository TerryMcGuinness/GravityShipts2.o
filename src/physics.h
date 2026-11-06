#ifndef PHYSICS_H
#define PHYSICS_H

// PhysicsBody: the Newtonian core that lives UNDERNEATH the collision code.
// Ships, balls and pellets are all PhysicsBodies: each carries mass, velocity
// and restitution, and collides through one shared impulse method. Restitution
// for a contact combines the pair (min), so the SAME code gives a dull bounce
// off the ground and a lively one off ships/pads.
//
// This replaces the old per-object ad-hoc bounce routines (bounceOffConvex,
// bounceOffCircle, bounceOffBase, the ground reflect) with one law.

#include "geometry.h"   // Point2D
#include <cmath>        // fminf

// Sensible per-type defaults; each object's constructor passes its own.
const float BODY_RESTITUTION_DEFAULT = 0.6f;

class PhysicsBody {
public:
    // TODO (teaching/debug-draw mode): every PhysicsBody can render its own
    // vectors in the OpenGL view as a physics-learning aid for younger folks.
    //   - velocity: a coloured arrow from pos along `velocity` (length ~ speed)
    //   - contact normal n: a BLUE arrow at the contact point when resolveImpulse
    //     / bounceOffStatic fires, showing the axis the impulse acts along
    //   - optional: the impulse j itself, and the resulting velocity change
    // Draw for ships, balls, pellets alike -- anything that IS a PhysicsBody.
    // Toggle with a key (debug HUD). Blue line + arrowhead for norms; the whole
    // point is to SEE that the normal is a function of position+velocity, not a
    // hand-set constant. Arrow helper: a short quad/line + two-line chevron head.
    Point2D pos      {0.0f, 0.0f};
    Point2D velocity {0.0f, 0.0f};
    float   mass        = 1.0f;   // kg (game units); drives momentum transfer
    float   restitution = BODY_RESTITUTION_DEFAULT; // e: 1=elastic, 0=dead
    float   drag        = 1.0f;   // 1.0 = no damping; <1 bleeds speed each frame
    // Phase 2: rotation. angVel in rad/frame; inertia is the moment of inertia
    // about the body's centre (kg*units^2). Off-centre impulses split into a
    // shove and a spin according to the lever arm r (contact - centre).
    float   angVel      = 0.0f;
    float   inertia     = 1.0f;

    // --- Constructors -------------------------------------------------------
    PhysicsBody() = default;

    PhysicsBody(float mass_, Point2D pos_, Point2D vel_, float restitution_)
        : pos(pos_), velocity(vel_), mass(mass_), restitution(restitution_) {}

    virtual ~PhysicsBody() = default;

    float invMass()    const { return mass    > 0.0f ? 1.0f / mass    : 0.0f; }
    float invInertia() const { return inertia > 0.0f ? 1.0f / inertia : 0.0f; }

    // 2D cross products.
    static float cross(Point2D a, Point2D b) { return a.x * b.y - a.y * b.x; }
    // Velocity of a point at lever arm r on this body: v + w x r.
    Point2D velocityAt(Point2D r) const { return { velocity.x - angVel * r.y, velocity.y + angVel * r.x }; }

    // Apply an impulse (momentum, kg*units/frame): dv = J / m. Thrust, recoil,
    // explosions all go through here so mass matters everywhere.
    void applyImpulse(Point2D J) {
        float im = invMass();
        velocity.x += J.x * im;
        velocity.y += J.y * im;
    }

    // Impulse J applied at lever arm r from the centre: linear dv = J/m plus
    // angular dw = (r x J)/I. r = 0 is a pure shove.
    void applyImpulseAt(Point2D J, Point2D r) {
        applyImpulse(J);
        angVel += cross(r, J) * invInertia();
    }

    // Integrate one frame: apply gravity (if any) then move (semi-implicit Euler).
    void integrate(float gravity) {
        velocity.x *= drag;            // per-body linear damping (1.0 = none)
        velocity.y *= drag;
        velocity.y -= gravity;
        pos.x += velocity.x;
        pos.y += velocity.y;
    }

    // Resolve a Newtonian impulse between two bodies along a unit normal `n`
    // (pointing from `other` toward `this`). Writes both velocities.
    // Returns false if the pair is already separating (no impulse, no stick).
    // This is the exact law lifted from Ship::resolveCollision, now shared.
    bool resolveImpulse(PhysicsBody& other, Point2D n) {
        return resolveImpulseAt(other, n, {0.0f, 0.0f}, {0.0f, 0.0f});
    }

    // Phase 2: the coupled linear+angular impulse through a contact point.
    // rA / rB are the lever arms from each body's centre to the contact. The
    // relative velocity AT the contact (including spin) decides whether the
    // pair is closing; the effective mass gains the (r x n)^2 / I terms so a
    // nose hit spends part of the impulse on spin and shoves less.
    //   j = -(1+e) vn / ( 1/mA + 1/mB + (rA x n)^2/IA + (rB x n)^2/IB )
    bool resolveImpulseAt(PhysicsBody& other, Point2D n, Point2D rA, Point2D rB) {
        Point2D va = velocityAt(rA), vb = other.velocityAt(rB);
        float vn = (va.x - vb.x) * n.x + (va.y - vb.y) * n.y;
        if (vn >= 0.0f) return false;            // separating: leave it alone

        float ran = cross(rA, n), rbn = cross(rB, n);
        float k = invMass() + other.invMass()
                + ran * ran * invInertia() + rbn * rbn * other.invInertia();
        if (k == 0.0f) return false;             // two immovable bodies

        float e = fminf(restitution, other.restitution);   // pair combines
        float j = -(1.0f + e) * vn / k;

        applyImpulseAt({ j * n.x,  j * n.y }, rA);
        other.applyImpulseAt({ -j * n.x, -j * n.y }, rB);
        return true;
    }

    // Bounce off a static surface (ground/pad treated as infinite mass) with a
    // contact restitution. One body, one normal -> reflect the normal component.
    bool bounceOffStatic(Point2D n, float contactRestitution) {
        float vn = velocity.x * n.x + velocity.y * n.y;
        if (vn >= 0.0f) return false;            // moving away: no bounce
        velocity.x -= (1.0f + contactRestitution) * vn * n.x;
        velocity.y -= (1.0f + contactRestitution) * vn * n.y;
        return true;
    }
};

#endif // PHYSICS_H
