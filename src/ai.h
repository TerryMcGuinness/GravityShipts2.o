#pragma once

#include "ship.h"
#include <array>
#include <cstdint>

namespace ai {

enum class Goal { Collect, Land, Refuel, Attack, Evade, Block };
enum class Phase { Launch, Transit, Brake, Approach, Align, Descend, Settle };
enum class Rotation { None, Clockwise, CounterClockwise };

struct ShipControls {
    Rotation rotation = Rotation::None;
    bool thrust = false;
    bool fire = false;
};

struct ShipView {
    Point2D position{}, velocity{};
    float heading = 0, spin = 0, fuel = 0;
    int score = 0, ammo = 0, cooldown = 0, id = 0;
    bool carrying = false, onPad = false, landed = false;
    ShipBall ball{};
    std::array<Point2D, SHIP> hull{};
    std::array<Point2D, BASE> pad{};
    std::array<Pellet, PELLET_MAX> pellets{};
};

struct Observation {
    ShipView self, opponent;
    std::uint64_t frame = 0;
};

struct Decision {
    Goal goal = Goal::Collect;
    float aggression = 0;
    bool threat = false;
};

Observation observe(const Ship& self, const Ship& opponent, std::uint64_t frame);
void apply(Ship& ship, const ShipControls& controls);
float wrappedDelta(float dx);
float heading(const Point2D& nose);
const char* goalName(Goal goal);

class Controller {
public:
    ShipControls update(const Observation& observation);
    Decision chooseGoal(const Observation& observation);
    // Ideal steering, then limited to human-like key timing.
    ShipControls fly(const Observation& observation, const Decision& decision);
    ShipControls steer(const Observation& observation, const Decision& decision);
    ShipControls humanize(const ShipControls& wanted, const ShipView& self);
    float difficulty() const { return level; }
    float targetDifficulty() const { return targetLevel; }
    const char* difficultyName() const;
    Decision decision() const { return current; }
    Phase phase() const { return flightPhase; }

private:
    int timing(int easyFrames, int hardFrames) const;
    Decision current{};
    Phase flightPhase = Phase::Launch;
    float level = 0, targetLevel = 0;
    float thrustCredit = 0;
    bool refueling = false;
    std::uint64_t goalSince = 0, combatAfter = 0, lastSelection = 0;
    Point2D trackedTarget{};
    Goal trackedGoal = Goal::Collect;
    float closestDistance = 1e9f;
    std::uint64_t progressFrame = 0, recoveryUntil = 0;
    bool tracking = false;
    bool thrustHeld = false;
    int thrustFrames = 1000;
    Rotation turnHeld = Rotation::None, lastTurn = Rotation::None;
    int turnFrames = 1000;
};

}
