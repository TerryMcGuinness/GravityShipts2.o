#pragma once

namespace sfx {

enum Event {
    Thud,       // ground / pad-top bounce, param = impact strength 0..1
    Clank,      // bump into side of a landing pad
    Clang,      // ship-ship collision
    Chime,      // clean landing on pad
    Collect,    // picked up own ball
    Score,      // point scored
    Boing,      // bounced off the other ship's ball
    Refuel,     // refuel tick, param = fuel fraction 0..1
    Explosion,  // loser blows up
    Win,        // victory fanfare
    Alarm,      // low fuel
    Sputter,    // thrust with empty tank
    Start,      // game start power-up
    Fire,       // pellet launched
    Dry,        // fire with empty magazine
    Ping,       // pellet hit a ship
    Reload,     // round added, param = magazine fraction 0..1
    NumEvents
};

bool init();
void shutdown();

// x is world x (-1.75..1.75) used for stereo pan; ship is 0/1, or -1 for shared.
void play(Event e, float x = 0.f, float param = 1.f, int ship = -1);

// Continuous per-ship engine state, call once per frame. rot: -1 CCW, 0 none, +1 CW.
void engine(int ship, bool thrust, int rot, float x);

}
