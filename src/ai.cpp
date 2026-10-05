#include "ai.h"
#include <algorithm>
#include <cmath>

namespace ai {
using namespace ai_tuning;
namespace {
constexpr float pi = 3.14159265358979323846f;
const float reserve = RESERVE_FRACTION * FUEL_CAPACITY;
const float refill = REFILL_FRACTION * FUEL_CAPACITY;

float clamp(float v, float lo, float hi) { return std::clamp(v, lo, hi); }
float angleDelta(float angle) { return std::remainder(angle, 2 * pi); }
float length(Point2D p) { return std::hypot(p.x, p.y); }
Point2D difference(Point2D to, Point2D from) {
    return {wrappedDelta(to.x - from.x), to.y - from.y};
}
bool combat(Goal goal) {
    return goal == Goal::Attack || goal == Goal::Block || goal == Goal::Evade;
}

ShipView snapshot(const Ship& ship) {
    ShipView v;
    v.position = ship.spaceShip.offset;
    v.velocity = ship.velocity;
    v.heading = heading(ship.spaceShip.vertices[0]);
    v.spin = ship.ang;
    v.fuel = ship.fuel;
    v.score = ship.score;
    v.ammo = ship.ammo;
    v.cooldown = ship.fireCooldown;
    v.id = ship.id;
    v.carrying = ship.spaceShip.ball.ballhit;
    v.onPad = ship.onPad;
    v.landed = ship.landed;
    v.ball = ship.spaceShip.ball;
    std::copy_n(ship.spaceShip.location, SHIP, v.hull.begin());
    std::copy_n(ship.spaceShip.landingPad, BASE, v.pad.begin());
    for (int i = 0; i < PELLET_MAX; ++i)
        if (ship.pellets[i].active) v.pellets[i] = ship.pellets[i];
    return v;
}

bool threatened(const Observation& o) {
    Point2D d = difference(o.opponent.position, o.self.position);
    Point2D dv = {o.opponent.velocity.x - o.self.velocity.x,
                  o.opponent.velocity.y - o.self.velocity.y};
    float vv = dv.x * dv.x + dv.y * dv.y;
    float t = vv > 0 ? clamp(-(d.x * dv.x + d.y * dv.y) / vv, 0, 40) : 0;
    if (length({d.x + t * dv.x, d.y + t * dv.y}) < 0.09f &&
        (d.x * dv.x + d.y * dv.y < 0 || length(d) < 0.06f))
        return true;
    for (const ShipView* owner : {&o.self, &o.opponent}) {
        for (const Pellet& p : owner->pellets) {
            if (!p.active || p.resting) continue;
            for (int frames = 4; frames <= 32; frames += 4) {
                if (owner == &o.self && p.life + frames < PELLET_ARM_FRAMES) continue;
                float x = wrappedDelta(p.pos.x + frames * p.vel.x -
                                       o.self.position.x - frames * o.self.velocity.x);
                float y = p.pos.y + frames * p.vel.y -
                          PELLET_GRAVITY * frames * (frames + 1) * 0.5f -
                          o.self.position.y - frames * o.self.velocity.y;
                if (std::hypot(x, y) < 0.045f) return true;
            }
        }
    }
    return false;
}

Point2D padTarget(const ShipView& s) {
    // The initial pad spans the world; landing directly below saves a traverse.
    float x = (s.pad[1].x + s.pad[2].x) * 0.5f;
    if (s.pad[2].x - s.pad[1].x > 3.0f) x = s.position.x;
    return {x, s.pad[1].y + 0.02f + 0.0003f};
}

Point2D shotDirection(const Observation& o) {
    Point2D d = difference(o.opponent.position, o.self.position);
    float t = clamp(length(d) / MUZZLE_SPEED, 1, 80);
    for (int i = 0; i < 3; ++i) {
        Point2D aim = {
            d.x + (o.opponent.velocity.x - o.self.velocity.x) * t,
            d.y + (o.opponent.velocity.y - o.self.velocity.y) * t +
                (PELLET_GRAVITY - (o.opponent.landed ? 0 : GRAVITY)) * t * t * 0.5f};
        t = clamp(length(aim) / MUZZLE_SPEED, 1, 80);
    }
    return {d.x + (o.opponent.velocity.x - o.self.velocity.x) * t,
            d.y + (o.opponent.velocity.y - o.self.velocity.y) * t +
                (PELLET_GRAVITY - (o.opponent.landed ? 0 : GRAVITY)) * t * t * 0.5f};
}

// Route above raised pads, or first sideways when starting underneath one.
void avoidPad(const ShipView& s, const std::array<Point2D, BASE>& pad, Point2D& target) {
    if (pad[1].y <= GROUND + 0.01f) return;
    if (std::max(s.position.y, target.y) < pad[0].y - 0.06f) return;
    float center = (pad[1].x + pad[2].x) * 0.5f;
    float half = (pad[2].x - pad[1].x) * 0.5f;
    float dx = wrappedDelta(center - s.position.x);
    float tx = wrappedDelta(target.x - s.position.x);
    bool crossing = (dx >= std::min(0.f, tx) - half - 0.06f &&
                     dx <= std::max(0.f, tx) + half + 0.06f);
    if (!crossing || std::min(target.y, s.position.y) > pad[1].y + 0.025f) return;
    if (target.y < pad[1].y + 0.025f && s.position.y > pad[0].y - 0.07f) {
        float targetSide = wrappedDelta(target.x - center);
        float side = std::fabs(dx) > half + 0.1f ? -dx : targetSide;
        if (std::fabs(side) < 0.001f) side = 1;
        target.x = center + (side < 0 ? -1 : 1) * (half + 0.20f);
        if (std::fabs(dx) < half + 0.12f)
            target.y = std::max(s.position.y, pad[1].y + APPROACH_HEIGHT);
        else
            target.y = std::min(target.y, pad[0].y - 0.09f);
        return;
    }
    if (s.position.y < pad[1].y + 0.06f && std::fabs(dx) < half + 0.13f) {
        target.x = center + (dx > 0 ? -1 : 1) * (half + 0.23f);
        target.y = std::min(s.position.y, pad[0].y - 0.08f);
    } else {
        target.y = std::max(target.y, pad[1].y + APPROACH_HEIGHT);
        if (s.position.y < pad[1].y + 0.07f)
            target.x = s.position.x;
    }
}
}

float wrappedDelta(float dx) { return std::remainder(dx, 3.5f); }
float heading(const Point2D& nose) { return std::atan2(-nose.x, nose.y); }

Observation observe(const Ship& self, const Ship& opponent, std::uint64_t frame) {
    return {snapshot(self), snapshot(opponent), frame};
}

void apply(Ship& ship, const ShipControls& controls) {
    if (controls.rotation == Rotation::CounterClockwise) ship.rotateCounterClockWise();
    else if (controls.rotation == Rotation::Clockwise) ship.rotateClockWise();
    if (controls.thrust) ship.thrust();
    if (controls.fire) ship.fire();
}

const char* goalName(Goal goal) {
    switch (goal) {
        case Goal::Collect: return "Collect";
        case Goal::Land: return "Land";
        case Goal::Refuel: return "Refuel";
        case Goal::Attack: return "Attack";
        case Goal::Evade: return "Evade";
        case Goal::Block: return "Block";
    }
    return "Unknown";
}

const char* Controller::difficultyName() const {
    return level < 0.5f ? "Easy" : level < 1.5f ? "Normal" : "Hard";
}

Decision Controller::chooseGoal(const Observation& o) {
    const auto& s = o.self;
    bool threat = threatened(o);
    if (s.fuel < reserve) refueling = true;
    if (s.onPad && s.fuel >= refill && s.ammo == AMMO_CAPACITY) refueling = false;
    Goal next = s.carrying ? Goal::Land : Goal::Collect;
    if (threat) next = Goal::Evade;
    else if (refueling) next = Goal::Refuel;
    else if (!s.carrying && o.frame >= combatAfter && s.ammo > 0) {
        float distance = length(difference(o.opponent.position, s.position));
        if (combat(current.goal) && o.frame - goalSince < COMBAT_FRAMES &&
            (current.goal != Goal::Block || o.opponent.carrying) &&
            (current.goal != Goal::Evade || o.frame - goalSince < 40)) {
            next = current.goal;
        } else if (combat(current.goal)) {
            combatAfter = o.frame + COMBAT_REST_FRAMES;
        } else if (o.opponent.carrying && distance < 0.8f + level * 0.25f) {
            next = Goal::Block;
        } else if (distance < 0.35f + level * 0.18f && s.position.y > GROUND + 0.2f) {
            next = Goal::Attack;
        }
    }
    if (next != current.goal) {
        if (combat(current.goal)) combatAfter = o.frame + COMBAT_REST_FRAMES;
        goalSince = o.frame;
    }
    return {next, clamp(level * 0.4f + 0.15f, 0, 1), threat};
}

ShipControls Controller::update(const Observation& o) {
    if (o.self.score >= WIN_SCORE || o.opponent.score >= WIN_SCORE) return {};
    targetLevel = clamp(float(o.opponent.score - o.self.score), 0, 2);
    level += clamp(targetLevel - level, -2.f / ADAPT_FRAMES, 2.f / ADAPT_FRAMES);
    bool invalid = (current.goal == Goal::Collect && o.self.carrying) ||
                   (current.goal == Goal::Land && !o.self.carrying) ||
                   ((current.goal == Goal::Attack || current.goal == Goal::Block) && o.self.carrying) ||
                   (current.goal == Goal::Block && !o.opponent.carrying) ||
                   (o.self.fuel < reserve && current.goal != Goal::Refuel);
    if (o.frame == 0 || o.frame - lastSelection >= STRATEGY_FRAMES ||
        invalid || threatened(o)) {
        current = chooseGoal(o);
        lastSelection = o.frame;
    }
    return fly(o, current);
}

ShipControls Controller::fly(const Observation& o, const Decision& decision) {
    return humanize(steer(o, decision), o.self);
}

int Controller::timing(int easy, int hard) const {
    return int(std::lround(easy + (hard - easy) * clamp(level / 2, 0, 1)));
}

ShipControls Controller::humanize(const ShipControls& wanted, const ShipView& s) {
    const int minBurst = timing(EASY_MIN_BURST_FRAMES, HARD_MIN_BURST_FRAMES);
    const int keyGap = timing(EASY_KEY_GAP_FRAMES, HARD_KEY_GAP_FRAMES);
    const int minTurn = timing(EASY_MIN_TURN_FRAMES, HARD_MIN_TURN_FRAMES);
    const int counterGap = timing(EASY_COUNTER_TURN_FRAMES, HARD_COUNTER_TURN_FRAMES);
    // Running dry or settling on a pad lets go at once. Just above the pad the
    // hands stay off the keys: no new presses, though held ones finish their minimum.
    const float center = (s.pad[1].x + s.pad[2].x) * 0.5f, half = (s.pad[2].x - s.pad[1].x) * 0.5f;
    const float clearance = std::min(s.hull[2].y, s.hull[4].y) - s.pad[1].y;
    const bool touchdown = !s.landed && !s.onPad && s.velocity.y <= 0 &&
                           clearance > -0.002f && clearance < 0.012f &&
                           std::fabs(wrappedDelta(s.position.x - center)) < half;
    const bool release = s.fuel < 0 || s.onPad || s.landed;
    ShipControls out = wanted;
    if (touchdown) {
        out.thrust = false;
        out.rotation = Rotation::None;
    }

    if (thrustHeld && !out.thrust && (thrustFrames >= minBurst || release)) {
        thrustHeld = false;
        thrustFrames = 0;
    } else if (!thrustHeld && out.thrust && thrustFrames >= keyGap) {
        thrustHeld = true;
        thrustFrames = 0;
    }
    ++thrustFrames;
    // Keep the duty-cycle accounting honest so longer bursts are followed by longer rests.
    if (thrustHeld != wanted.thrust && !s.landed)
        thrustCredit = clamp(thrustCredit + (thrustHeld ? -1.f : 1.f), -8.f, 2.f);
    out.thrust = thrustHeld;

    Rotation turn = out.rotation;
    if (turnHeld != Rotation::None && turn != turnHeld && turnFrames < minTurn && !release)
        turn = turnHeld;
    if (turnHeld != Rotation::None && turn != Rotation::None && turn != turnHeld)
        turn = Rotation::None;   // let go before reversing
    if (turnHeld == Rotation::None && turn != Rotation::None && lastTurn != Rotation::None &&
        turnFrames < (turn == lastTurn ? keyGap : counterGap))
        turn = Rotation::None;
    if (turn != turnHeld) {
        if (turnHeld != Rotation::None) lastTurn = turnHeld;
        turnHeld = turn;
        turnFrames = 0;
    }
    ++turnFrames;
    out.rotation = turnHeld;
    return out;
}

ShipControls Controller::steer(const Observation& o, const Decision& decision) {
    ShipControls result;
    const ShipView& s = o.self;
    if (s.fuel < 0 || s.score >= WIN_SCORE || o.opponent.score >= WIN_SCORE) return result;
    bool landing = decision.goal == Goal::Land || decision.goal == Goal::Refuel;
    if (landing && s.onPad) {
        flightPhase = Phase::Settle;
        return result;
    }
    Point2D target = landing ? padTarget(s) : s.ball.ballLocation;
    if (decision.goal == Goal::Attack) {
        target = {o.opponent.position.x, o.opponent.position.y - 0.22f};
    } else if (decision.goal == Goal::Block) {
        target = o.opponent.carrying ? padTarget(o.opponent) : o.opponent.ball.ballLocation;
        target.y += 0.16f;
    } else if (decision.goal == Goal::Evade) {
        float away = wrappedDelta(s.position.x - o.opponent.position.x) < 0 ? -1.f : 1.f;
        target = {s.position.x + away * 0.22f, s.position.y + 0.12f};
    }
    float distance = length(difference(target, s.position));
    if (!tracking || trackedGoal != decision.goal ||
        length(difference(target, trackedTarget)) > 0.025f ||
        distance < closestDistance - 0.005f) {
        tracking = true;
        trackedGoal = decision.goal;
        trackedTarget = target;
        closestDistance = distance;
        progressFrame = o.frame;
    }
    if (!combat(decision.goal) && o.frame - progressFrame > STALL_FRAMES) {
        recoveryUntil = o.frame + RECOVERY_FRAMES;
        progressFrame = o.frame;
        closestDistance = 1e9f;
    }
    bool recovering = o.frame < recoveryUntil;
    if (recovering) {
        landing = false;
        target = {s.position.x + (s.id == 0 ? -0.15f : 0.15f), s.position.y + 0.2f};
    }
    target.y = clamp(target.y, GROUND + 0.0203f, 0.92f);
    float dx = wrappedDelta(target.x - s.position.x);
    float deck = s.pad[1].y;
    bool aligned = landing && std::fabs(dx) < 0.015f &&
                   std::fabs(s.velocity.x) < 0.0003f;
    flightPhase = s.landed ? Phase::Launch : Phase::Transit;
    if (landing) {
        if (!aligned || s.position.y < deck - 0.01f) {
            target.y = std::max(target.y + APPROACH_HEIGHT, deck + APPROACH_HEIGHT);
            flightPhase = Phase::Approach;
            avoidPad(s, s.pad, target);
        } else {
            flightPhase = std::fabs(s.heading) > 0.03f ? Phase::Align : Phase::Descend;
            if (flightPhase == Phase::Align) target.y = std::max(target.y, deck + 0.07f);
        }
    } else {
        avoidPad(s, s.pad, target);
    }
    // Identical initial pads overlap; do not turn the other pad into a phantom obstacle.
    if (std::fabs(o.opponent.pad[1].x - s.pad[1].x) > 0.001f ||
        std::fabs(o.opponent.pad[1].y - s.pad[1].y) > 0.001f)
        avoidPad(s, o.opponent.pad, target);
    dx = wrappedDelta(target.x - s.position.x);
    float dy = target.y - s.position.y;
    float maxSpeed = CRUISE_SPEED + level * LEVEL_SPEED;
    float desiredX = clamp(dx * POSITION_GAIN, -maxSpeed, maxSpeed);
    float desiredY = clamp(dy * POSITION_GAIN, -maxSpeed, maxSpeed);
    if (landing && aligned) desiredY = clamp(dy * 0.015f, -DESCENT_SPEED, maxSpeed);
    if (std::fabs(dx) < 0.15f && !landing) flightPhase = Phase::Brake;
    float ax = clamp((desiredX - s.velocity.x) * HORIZONTAL_GAIN,
                     -MAX_HORIZONTAL_ACCELERATION, MAX_HORIZONTAL_ACCELERATION);
    float ay = GRAVITY + (desiredY - s.velocity.y) * VERTICAL_GAIN;
    float desiredHeading = clamp(std::atan2(-ax, std::max(ay, GRAVITY)), -MAX_TILT, MAX_TILT);
    if (landing && aligned && s.position.y < deck + 0.06f)
        desiredHeading = clamp(desiredHeading, -0.012f, 0.012f);

    Point2D shot = shotDirection(o);
    float range = length(difference(o.opponent.position, s.position));
    bool canAttack = !landing && !recovering && !s.carrying && s.ammo > 0 && s.cooldown == 0 &&
                     decision.goal != Goal::Evade && s.position.y > GROUND + 0.12f &&
                     range < 0.65f && std::fabs(s.spin) < 0.02f;
    if (decision.goal == Goal::Attack && canAttack && std::fabs(s.velocity.x) < 0.0025f &&
        s.velocity.y > -0.001f) {
        desiredHeading = clamp(heading(shot), -1.1f, 1.1f);
    }
    float error = angleDelta(desiredHeading - s.heading);
    float desiredSpin = clamp(error * 0.1f, -MAX_SPIN, MAX_SPIN);
    float brakingSpin = std::sqrt(2 * ANGLE_FACTOR * std::fabs(error)) * 0.65f;
    desiredSpin = clamp(desiredSpin, -brakingSpin, brakingSpin);
    // A tap can't be shorter than the minimum, so ignore errors smaller than half of one.
    float deadband = ANGLE_FACTOR * 0.5f * timing(EASY_MIN_TURN_FRAMES, HARD_MIN_TURN_FRAMES);
    // Over the pad, hold a near-level attitude instead of chasing every last hundredth.
    bool settling = flightPhase == Phase::Align || flightPhase == Phase::Descend;
    bool holdLevel = settling && std::fabs(error) < LEVEL_SLACK && std::fabs(s.spin) <= deadband;
    if (!holdLevel && desiredSpin - s.spin > deadband)
        result.rotation = Rotation::CounterClockwise;
    else if (!holdLevel && desiredSpin - s.spin < -deadband)
        result.rotation = Rotation::Clockwise;

    // Thrust acts along the current vertices, before this frame's rotation.
    float nx = -std::sin(s.heading), ny = std::cos(s.heading);
    float thrustAcceleration = 0.01f * VELOCITY_FACTOR;
    thrustCredit += ny > 0.3f ? clamp((nx * ax + ny * ay) / thrustAcceleration, 0, 1) : 0;
    result.thrust = thrustCredit >= 1 && ny > 0.3f;
    if (result.thrust) thrustCredit -= 1;
    if (s.landed && !landing) result.thrust = true;
    float aggression = clamp(decision.aggression, 0, 1);
    float tolerance = 0.08f - aggression * 0.045f;
    int interval = int(90 - aggression * 60);
    result.fire = canAttack && std::fabs(angleDelta(heading(shot) - s.heading)) < tolerance &&
                  o.frame % interval == 0;
    return result;
}

}
