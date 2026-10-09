#include "ai.h"
#include <cmath>
#include <iostream>

static int failures = 0;
static void check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; ++failures; }
}

static ai::Observation observation() {
    ai::Observation o;
    o.self.fuel = FUEL_CAPACITY;
    o.self.ammo = AMMO_CAPACITY;
    o.self.ball.ballLocation = {0.8f, 0.5f};
    o.self.pad = {{{-0.11f, -0.85f}, {-0.1f, -0.82f}, {0.1f, -0.82f}, {0.11f, -0.85f}}};
    o.opponent = o.self;
    o.opponent.position = {-1.3f, -0.9f};
    o.opponent.pad = o.self.pad;
    return o;
}

int main() {
    check(std::fabs(ai::wrappedDelta(3.3f) + 0.2f) < 0.00001f, "positive wrap");
    check(std::fabs(ai::wrappedDelta(-3.3f) - 0.2f) < 0.00001f, "negative wrap");
    check(std::fabs(ai::heading({-1, 0}) - 1.5707963f) < 0.00001f, "heading from nose");
    auto o = observation();
    ai::Controller c;
    check(c.difficulty() == 0, "starts easy");
    o.opponent.score = 2;
    for (o.frame = 0; o.frame < 180; ++o.frame) c.update(o);
    check(std::fabs(c.difficulty() - 2) < 0.0001f, "ramps to hard");
    o.opponent.score = 1;
    for (; o.frame < 360; ++o.frame) c.update(o);
    check(c.difficulty() == 1, "eases to normal");
    o.opponent.score = 0;
    for (; o.frame < 540; ++o.frame) c.update(o);
    check(c.difficulty() == 0, "eases to easy");
    o.self.spin = 0.03f;
    auto action = c.steer(o, {});
    check(action.rotation == ai::Rotation::Clockwise, "brakes positive spin");
    o.self.spin = -0.03f;
    action = c.steer(o, {});
    check(action.rotation == ai::Rotation::CounterClockwise, "brakes negative spin");
    o.self.spin = 0;
    o.self.carrying = true;
    c.update(o);
    check(c.decision().goal == ai::Goal::Land, "pickup invalidates collection");
    o.self.fuel = 600;
    ++o.frame;
    c.update(o);
    check(c.decision().goal == ai::Goal::Refuel, "fuel reserve overrides delivery");
    o.self.fuel = 900;
    o.frame += 20;
    c.update(o);
    check(c.decision().goal == ai::Goal::Refuel, "refuel hysteresis");
    o.self.onPad = true;
    o.self.fuel = 1900;
    o.frame += 20;
    c.update(o);
    check(c.decision().goal == ai::Goal::Land, "refueling finishes");
    o.self.fuel = -1;
    action = c.update(o);
    check(!action.thrust && !action.fire && action.rotation == ai::Rotation::None, "empty tank");
    o = observation();
    o.opponent.position = {0.2f, 0.1f};
    ai::Controller attack;
    attack.update(o);
    check(attack.decision().goal == ai::Goal::Attack, "attack opportunity");
    o.frame = 260;
    attack.update(o);
    check(attack.decision().goal == ai::Goal::Collect, "combat expires");
    o.frame = 280;
    attack.update(o);
    check(attack.decision().goal == ai::Goal::Collect, "combat rest");
    o.self.carrying = true;
    ++o.frame;
    attack.update(o);
    check(attack.decision().goal == ai::Goal::Land, "delivery takes priority over renewed combat");
    o = observation();
    o.opponent.position = {0.5f, 0.2f};
    o.opponent.carrying = true;
    ai::Controller block;
    block.update(o);
    check(block.decision().goal == ai::Goal::Block, "blocks delivery");
    o = observation();
    { Pellet pel; pel.pos = {0.1f, 0.002f}; pel.velocity = {-0.01f, 0};
      pel.life = 10; pel.active = true; pel.resting = false;
      o.opponent.pellets[0] = pel; }
    ai::Controller evade;
    evade.update(o);
    check(evade.decision().goal == ai::Goal::Evade, "evades predicted projectile");
    o.self.fuel = 100;
    ++o.frame;
    evade.update(o);
    check(evade.decision().goal == ai::Goal::Evade, "immediate threat overrides fuel return");
    o = observation();
    o.self.carrying = true;
    o.self.onPad = true;
    action = c.fly(o, {ai::Goal::Land, 1, false});
    check(!action.fire && !action.thrust, "settling suppresses recoil and launch");
    o = observation();
    ai::Controller stalled;
    for (o.frame = 0; o.frame < 800; ++o.frame) {
        action = stalled.update(o);
        check(action.rotation == ai::Rotation::None ||
              action.rotation == ai::Rotation::Clockwise ||
              action.rotation == ai::Rotation::CounterClockwise, "legal rotation output");
    }
    for (bool humanWins : {false, true}) {
        o = observation();
        (humanWins ? o.opponent : o.self).score = WIN_SCORE;
        action = c.update(o);
        check(!action.thrust && !action.fire && action.rotation == ai::Rotation::None,
              "terminal controls");
    }

    using namespace ai_tuning;
    using ai::Rotation;
    auto burst = [](ai::Controller& hands, const ai::ShipView& self) {
        ai::ShipControls tap;
        tap.thrust = true;
        int on = hands.humanize(tap, self).thrust;
        for (int i = 0; i < 30; ++i) on += hands.humanize({}, self).thrust;
        return on;
    };
    o = observation();
    ai::Controller easy;
    check(burst(easy, o.self) == EASY_MIN_BURST_FRAMES, "easy stretches a one-frame burst");
    ai::ShipControls tap;
    tap.thrust = true;
    ai::Controller pulser;
    pulser.humanize(tap, o.self);
    while (pulser.humanize({}, o.self).thrust) {}
    int offFrames = 1;
    while (!pulser.humanize(tap, o.self).thrust && offFrames < 30) ++offFrames;
    check(offFrames == EASY_KEY_GAP_FRAMES, "easy pauses between bursts");

    ai::Controller turner;
    ai::ShipControls cw, ccw;
    cw.rotation = Rotation::Clockwise;
    ccw.rotation = Rotation::CounterClockwise;
    int held = turner.humanize(cw, o.self).rotation == Rotation::Clockwise;
    int idle = 0;
    ai::ShipControls out;
    while ((out = turner.humanize(ccw, o.self)).rotation == Rotation::Clockwise) ++held;
    while (out.rotation == Rotation::None && idle < 40) {
        ++idle;
        out = turner.humanize(ccw, o.self);
    }
    check(held == EASY_MIN_TURN_FRAMES, "easy holds a rotation tap");
    check(idle == EASY_COUNTER_TURN_FRAMES, "easy pauses before counter-rotating");
    check(out.rotation == Rotation::CounterClockwise, "counter-rotation follows the pause");

    ai::Controller hard;
    o = observation();
    o.opponent.score = 2;
    for (o.frame = 0; o.frame < 180; ++o.frame) hard.update(o);
    for (int i = 0; i < 20; ++i) hard.humanize({}, o.self);
    check(burst(hard, o.self) == HARD_MIN_BURST_FRAMES, "hard allows short but not single-frame bursts");

    ai::Controller touchdown;
    o = observation();
    touchdown.humanize(tap, o.self);
    o.self.onPad = true;
    check(!touchdown.humanize({}, o.self).thrust, "touchdown releases thrust immediately");
    return failures ? 1 : 0;
}
