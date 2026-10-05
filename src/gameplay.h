#pragma once

#include "ship.h"

void resetBall(Ship& ship);
void initialShipPosition(Ship& ship1, Ship& ship2);
void resetLandingBase(Ship& ship);
bool updateShip(Ship& ship);
void resolveShipCollision(Ship& ship1, Ship& ship2);
