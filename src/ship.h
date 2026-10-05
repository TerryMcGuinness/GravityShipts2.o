//
//  ship.h
//  opengl-series
//
//  Created by Terrence McGuinness on 3/14/15.
//
//

#ifndef __opengl_series__ship__
#define __opengl_series__ship__

#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <string>
#include <unistd.h>

#include "sound.h"

extern GLuint cloudTexture;

// Removes the cloud image's grey background and feathers it to a disc for additive glow.
void prepareCloudTexture(unsigned char* rgba, int w, int h);

enum {
    STAGE1,
    STAGE2,
    STAGE3,
    STAGE4,
    STAGE5
};


// testing update

const float FUEL_ROTATE =   0.20;
const float FUEL_THRUST =   0.10;
const float FUEL_CAPACITY  = 2000.0;
const float REFUEL_RATE    = 20.0;
const float BASE_THICKNESS = 0.015;
const float GRAVITY        = 0.00001;
const float VELOCITY_FACTOR = 0.008;
const float ANGLE_FACTOR    = 0.0004;

const float GROUND = -1.0;
const int   WIN_SCORE = 5;

const int   PELLET_MAX         = 24;
const int   AMMO_CAPACITY      = 6;
const int   FIRE_COOLDOWN      = 12;     // frames between shots
const int   AMMO_RECHARGE      = 45;     // frames per round while sitting on own pad
const int   PELLET_REST_FRAMES = 900;    // spent rounds lie on the ground this long
const float PELLET_RADIUS      = 0.004;
const float PELLET_GRAVITY     = 0.0001;
const float MUZZLE_SPEED       = 0.010;
const float RECOIL             = 0.0006;
const float PELLET_KICK        = 0.25;   // fraction of pellet momentum given to the target
const float PELLET_SPIN_KICK   = 8.0;
const float PICKUP_RADIUS      = 0.03;
const float PELLET_BOUNCE      = 0.6;    // coefficient of restitution
const int   PELLET_ARM_FRAMES  = 8;      // own ship is immune until the round clears the muzzle

const int   EXPLOSION_SPARKS      = 96;
const int   EXPLOSION_GROW_FRAMES = 330;    // cloud blooms from a seed to full size
const float EXPLOSION_CLOUD_SIZE  = 0.19;   // full-grown cloud radius
const float EXPLOSION_CLOUD_SPIN  = 0.15;   // degrees per frame
const int   EXPLOSION_RING_FRAMES = 55;     // shockwave lifetime

namespace ai_tuning {
constexpr int STRATEGY_FRAMES = 20;
constexpr int COMBAT_FRAMES = 240;
constexpr int COMBAT_REST_FRAMES = 480;
constexpr int ADAPT_FRAMES = 180;
constexpr int STALL_FRAMES = 600;
constexpr int RECOVERY_FRAMES = 120;
constexpr float RESERVE_FRACTION = 0.35f;
constexpr float REFILL_FRACTION = 0.9f;
constexpr float APPROACH_HEIGHT = 0.14f;
constexpr float CRUISE_SPEED = 0.0015f;
constexpr float LEVEL_SPEED = 0.0004f;
constexpr float DESCENT_SPEED = 0.00045f;
constexpr float POSITION_GAIN = 0.008f;
constexpr float HORIZONTAL_GAIN = 0.035f;
constexpr float VERTICAL_GAIN = 0.06f;
constexpr float MAX_HORIZONTAL_ACCELERATION = 0.000022f;
constexpr float MAX_TILT = 0.9f;
constexpr float MAX_SPIN = 0.035f;
// Human-like key timing. Interpolated from Easy to Hard, so the CPU only gets
// finer control as it falls behind; even Hard never pulses for a single frame.
constexpr int EASY_MIN_BURST_FRAMES = 6;    // shortest thrust burst
constexpr int HARD_MIN_BURST_FRAMES = 2;
constexpr int EASY_KEY_GAP_FRAMES = 4;      // shortest pause before pressing again
constexpr int HARD_KEY_GAP_FRAMES = 1;
constexpr int EASY_MIN_TURN_FRAMES = 5;     // shortest rotation tap
constexpr int HARD_MIN_TURN_FRAMES = 2;
constexpr int EASY_COUNTER_TURN_FRAMES = 9; // pause before a counter-rotation
constexpr int HARD_COUNTER_TURN_FRAMES = 2;
constexpr float LEVEL_SLACK = 0.015f;       // radians left uncorrected while settling onto a pad
}

struct Point2D {
    GLfloat x;
    GLfloat y;
    
    Point2D operator +(const Point2D &a)
    {
        return{a.x+x,a.y+y};
    }
    
    Point2D operator *(const Point2D &a)
    {
        return{a.x*x,a.y*y};
    }
    Point2D& operator =(const Point2D &a)
    {
        x = a.x ;
        y = a.y ;
        return *this;
    }

};

struct ShipStatusConsts {
    int statTextPosX;
    int statTextPosY;
    float scoreLocation;
};

struct ShipBall {
    Point2D ballLocation ;
    float ballSize ;
    bool ballhit ;
    bool ballhitonce ;
};

struct Pellet {
    Point2D pos;
    Point2D vel;
    int  life;
    bool active;
    bool resting;
};

struct Debris {          // one hull edge flung free, endpoints relative to pos
    Point2D pos, vel, a, b;
    float ang, spin;
};

struct Spark {
    Point2D pos, vel;
    int  life, maxLife;
    bool ember;          // slow rising ember rather than a blast spark
};

#define BASE_THICKNESS 0.01


#define SHIP 6
#define BASE 4
struct SpaceShip {
    Point2D vertices[SHIP];
    Point2D location[SHIP];
    Point2D landingPad[BASE];
    Point2D offset;
    ShipBall ball;
};


float
randomRange(float min, float max);

void drawText(std::string text, int x, int y);

class Ship {

private:
    
    float velocityFactor = VELOCITY_FACTOR;
    float angFactor      = ANGLE_FACTOR;
    float gravity        = GRAVITY;
    
    void rotate(float ang);
    Point2D rotatedPoints[SHIP];
    Point2D rotatePoints(Point2D p, float ang);
    
    float pi = 3.1415926;
    int tailPoints = 3;
    Point2D tail[3];
    
    int statTextPosX,statTextPosY;
    float scoreLocation;
    
    int thrustPoints = 2;
    Point2D leftThrust[2];
    Point2D rightThrust[2];
    GLfloat shipColor[3];
    bool shipInPolygon(SpaceShip,Point2D*);
    bool bounceOffBase(const Point2D*);

public:
    
    bool hitSide ;
    

    int  id = 0;            // 0/1, for sound voicing
    int  rotDir = 0;        // set by rotate*() this frame
    bool thrusting = false; // set by thrust() this frame
    Ship(ShipStatusConsts);
    ~Ship();
   
    SpaceShip spaceShip;
    void reset(Point2D offset);
    void rotateShip(void);
    void thrust(void);
    void rotateCounterClockWise(void);
    void rotateClockWise(void);
    void updatePosition(void);
    void inBounds(void);
    void explode(void);
    GLfloat rotateCloud ;
    int explodeFrame ;
    Debris debris[SHIP];
    Spark sparks[EXPLOSION_SPARKS];
    
    void setBallLocation( Point2D ballLocation, float ballSize);
    void drawBall(void);
    void setLandingLocation(Point2D location, float size);
    void drawLandingPad(void);
    void storeOtherShip( SpaceShip spaceShip) ;
    bool ifShipsColide(void);
    
    Point2D otherShipsPosition[SHIP];
    Point2D otherShipPad[BASE];

    ShipBall otherShipBall;
    bool hitOtherBall;
    
    float ang ;
    Point2D velocity ;
    void drawShip();
    void setShipColor(GLfloat* color);
    void drawCircle(float r);    
    
    Point2D landingLocation;
    float landingPadSize ;
    
    bool landedOnPad ;
    bool onPad ;
    bool landed;
     
    bool isRefueling;
    int increaseFuelInc;

    Pellet pellets[PELLET_MAX];
    int  ammo;
    int  fireCooldown;
    int  ammoRecharge;
    void fire(void);
    void updatePellets(Ship& other);
    void drawPellets(void);
    void bump(const Pellet& p);
    
    bool pointWasInPolygon ;

    int score ;
    void displayShipStatus(void);
    void displayShipScore(void);
    
    float fuel = FUEL_CAPACITY ;
};



#endif /* defined(__opengl_series__ship__) */
