#include "gameplay.h"
#include "sound.h"

void resetBall(Ship& ship) {
    Point2D ball;
    do {
        ball.x = randomRange(-1.3, 1.3);
        rand(); rand(); rand();
        ball.y = randomRange(-0.9, 0.9);
    } while (sqrt(pow(ball.x, 2) + pow(ball.y, 2)) <= 0.03);
    const float sizes[] = {0.05f, 0.04f, 0.03f, 0.02f, 0.01f};
    ship.setBallLocation(ball, sizes[ship.score]);
}

void initialShipPosition(Ship& ship1, Ship& ship2) {
    float diff = fabs(ship1.spaceShip.vertices[1].y - ship1.spaceShip.vertices[2].y) + 0.005;
    Point2D position = {randomRange(-1.3, 1.3), GROUND + diff};
    ship1.reset(position);
    Point2D base = {0.0f, GROUND + 0.02f};
    ship1.setLandingLocation(base, 2 * 1.75);
    float firstX = position.x;
    do {
        position.x = randomRange(-1.3, 1.3);
    } while (fabs(position.x - firstX) <= 0.05);
    ship2.reset(position);
    ship2.setLandingLocation(base, 2 * 1.75);
    ship1.storeOtherShip(ship2.spaceShip);
    ship2.storeOtherShip(ship1.spaceShip);
}

void resetLandingBase(Ship& ship) {
    Point2D point;
    do {
        point.x = randomRange(-1.3, 1.3);
        point.y = randomRange(-0.3, 0.75);
    } while (sqrt(pow(ship.otherShipsPosition[0].x - point.x, 2) +
                  pow(ship.otherShipsPosition[0].y - point.y, 2)) <= 0.05);
    if (ship.score == STAGE2) {
        point.y = GROUND + BASE_THICKNESS * 5;
        ship.setLandingLocation(point, 0.085);
    }
    if (ship.score == STAGE3) ship.setLandingLocation(point, 0.075);
    if (ship.score == STAGE4) ship.setLandingLocation(point, 0.065);
    if (ship.score == STAGE5) ship.setLandingLocation(point, 0.050);
}

static void refuelShip(Ship& ship) {
    if (ship.isRefueling) ship.increaseFuelInc += 1;
    if (ship.increaseFuelInc > 30) {
        ship.increaseFuelInc = 0;
        sfx::play(sfx::Refuel, ship.spaceShip.offset.x, ship.fuel / FUEL_CAPACITY, ship.id);
        ship.fuel += REFUEL_RATE;
    }
}

bool updateShip(Ship& ship) {
    ship.inBounds();
    ship.updatePosition();
    ship.rotateShip();
    if (ship.score < WIN_SCORE && ship.spaceShip.ball.ballhit && !ship.spaceShip.ball.ballhitonce) {
        ship.spaceShip.ball.ballhitonce = true;
        resetLandingBase(ship);
    }
    ship.drawLandingPad();
    if (ship.score < WIN_SCORE && ship.landedOnPad &&
        ship.spaceShip.ball.ballhit && ship.spaceShip.ball.ballhitonce) {
        ship.score += 1;
        sfx::play(sfx::Score, ship.spaceShip.offset.x, 1.f, ship.id);
        ship.spaceShip.ball.ballhit = false;
        ship.spaceShip.ball.ballhitonce = false;
        if (ship.score < WIN_SCORE) resetBall(ship);
    }
    if (ship.score < WIN_SCORE && !ship.spaceShip.ball.ballhit) ship.drawBall();
    if (ship.onPad) {
        if (ship.fuel < FUEL_CAPACITY) {
            ship.isRefueling = true;
            refuelShip(ship);
        } else {
            ship.isRefueling = false;
            ship.fuel = FUEL_CAPACITY;
        }
    }
    if (ship.hitOtherBall) {
        ship.velocity.x *= -1;
        ship.velocity.y *= -1;
        if (ship.spaceShip.location[0].x < ship.spaceShip.ball.ballLocation.x)
            ship.spaceShip.offset.x -= 0.005;
        else
            ship.spaceShip.offset.x += 0.005;
        if (ship.spaceShip.location[0].y > ship.spaceShip.ball.ballLocation.y)
            ship.spaceShip.offset.y += 0.005;
        else
            ship.spaceShip.offset.y -= 0.005;
        ship.hitOtherBall = false;
    }
    return true;
}

void resolveShipCollision(Ship& ship1, Ship& ship2) {
    // Newtonian impulse (Phase 1, linear): conserves momentum, with restitution
    // for the elastic/inelastic range. Replaces the old velocity-flip bounce.
    if (ship1.resolveCollision(ship2))
        sfx::play(sfx::Clang, ship1.spaceShip.offset.x);
}
