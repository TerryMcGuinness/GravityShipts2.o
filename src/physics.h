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

    // --- Constructors -------------------------------------------------------
    PhysicsBody() = default;

    PhysicsBody(float mass_, Point2D pos_, Point2D vel_, float restitution_)
        : pos(pos_), velocity(vel_), mass(mass_), restitution(restitution_) {}

    virtual ~PhysicsBody() = default;

    float invMass() const { return mass > 0.0f ? 1.0f / mass : 0.0f; }

    // Integrate one frame: apply gravity (if any) then move.
    void integrate(float gravity) {
        velocity.y -= gravity;
        pos.x += velocity.x;
        pos.y += velocity.y;
    }

    // Resolve a Newtonian impulse between two bodies along a unit normal `n`
    // (pointing from `other` toward `this`). Writes both velocities.
    // Returns false if the pair is already separating (no impulse, no stick).
    // This is the exact law lifted from Ship::resolveCollision, now shared.
    bool resolveImpulse(PhysicsBody& other, Point2D n) {
        float rvx = velocity.x - other.velocity.x;
        float rvy = velocity.y - other.velocity.y;
        float vn  = rvx * n.x + rvy * n.y;
        if (vn >= 0.0f) return false;            // separating: leave it alone

        float invA = invMass(), invB = other.invMass();
        float invSum = invA + invB;
        if (invSum == 0.0f) return false;        // two immovable bodies

        float e = fminf(restitution, other.restitution);   // pair combines
        float j = -(1.0f + e) * vn / invSum;

        velocity.x       += (j * invA) * n.x;  velocity.y       += (j * invA) * n.y;
        other.velocity.x -= (j * invB) * n.x;  other.velocity.y -= (j * invB) * n.y;
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
