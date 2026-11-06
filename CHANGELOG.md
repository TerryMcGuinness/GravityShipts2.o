# Changelog

All notable changes to GravityShips2.o are recorded here.

This project is a fork of the original GravityShips (Terrence McGuinness,
originally authored ~2015), rebuilt as a Linux-native C++17/CMake game.

The format is loosely based on Keep a Changelog (https://keepachangelog.com/).
Newest entries go at the top. Every change to the project must be recorded here
as part of the same work (see .github/copilot-instructions.md).

## [Unreleased]

### Changed
- Ship motion now runs through PhysicsBody: updatePosition() syncs
  spaceShip.offset -> pos, calls integrate() (gravity 0 when landed, drag 1),
  syncs back and rebuilds the hull. Thrust is PhysicsBody::applyImpulse along
  the nose with SHIP_THRUST = VELOCITY_FACTOR * SHIP_MASS, so acceleration is
  F/m (feel unchanged at mass 1; heavier/damaged ships will fly heavier).
  Integration order is now semi-implicit Euler (gravity then move); at
  GRAVITY=1e-5 this is imperceptible and the AI scenarios still pass.
- PhysicsBody gains applyImpulse(J): dv = J/m, for thrust/recoil/explosions.
- collision_tests: gravity-through-integrate, landed=no-gravity, offset/hull
  sync, and double-mass-half-dv cases.
- Ship now derives from PhysicsBody. velocity/mass/restitution come from the
  base; Ship::resolveCollision keeps the SAT contact + mass-split separation and
  delegates the impulse to PhysicsBody::resolveImpulse. Position stays on
  spaceShip.offset (hull anchor); ships do not use the base integrate().
- The last four placeholder bounces are now the shared law (bounceOffStatic
  with the ship's restitution, so damage will read everywhere):
  ground (was velocity.y = -velocity.y), pad top (was *= -1), pad sides in
  bounceOffBase (was a 2*vn full-elastic reflect), and the other player's ball
  (was flipping BOTH velocity components; now reflects along the ball->ship
  normal and nudges the ship out along it).
- collision_tests: new case upcasting Ship to PhysicsBody& and checking a
  ground bounce keeps e=0.6 of the speed instead of 100%.

### Added (real_balls, merged to main)
- Pellet, Debris and Spark now derive from PhysicsBody: pos/velocity/mass/
  restitution/drag come from the base. Their four hand-rolled integrate-and-
  bounce loops are collapsed onto PhysicsBody::integrate() (gravity + per-body
  linear drag) and bounceOffStatic() (reflect with restitution). Per-body feel
  preserved: debris drag 0.99 / e 0.3, sparks 0.965 / 0.4, pellets 1.0 / 0.6.
- PhysicsBody gains a `drag` field applied in integrate() (1.0 = none).
- Pellets now BOUNCE off the ground instead of dead-stopping: ~3 bounces from a
  top-of-screen drop, ~2 from mid-screen (e=0.6), settling to resting below
  PELLET_REST_SPEED. Replaces the old velocity=0/resting-on-contact early-out.
- Ship::bump() is now mass-driven: a pellet's shove on a ship scales with the
  pellet/ship mass ratio and pair restitution instead of the hand-tuned
  PELLET_KICK. Changing Pellet mass now changes the bodoink automatically.
  Pellet mass tuned to 0.15 (vs ship 1.0) for a modest, playable knock.
  TODO left in bump() to later fold the ship shove + pellet ricochet into a
  single resolveImpulse() call.
- src/geometry.h: Point2D extracted from ship.h into its own header so the
  physics base can depend on it without pulling in Ship.
- src/physics.h: PhysicsBody base class -- the Newtonian core beneath the
  collision code. Carries pos, velocity, mass, restitution; provides
  integrate(), invMass(), resolveImpulse(other, n) (the shared impulse law
  lifted from Ship::resolveCollision: separating-pair early-out, pair-min
  restitution, momentum-conserving impulse) and bounceOffStatic(n, e) for
  infinite-mass ground/pads. Ships, balls, pellets will all become
  PhysicsBodies.
- tests/physics_tests.cpp (physics_unit): 6 cases -- elastic e=1 conserves
  momentum AND kinetic energy; inelastic e=0 conserves momentum, loses energy;
  light ball vs heavy ship conserves momentum with the ship barely moving;
  separating pair gets no impulse; dull ground (e=0.2) vs lively pad (e=0.9)
  static bounce. All 4 CTest suites pass 100%.
- TODO in physics.h (LATER, display-only teaching/debug-draw mode): every
  PhysicsBody can render its velocity vector and contact normal in the GL view
  as a physics-learning aid; for ships, the moving tangent/normal frame plus a
  clothoid (Euler-spiral) retro-thrust demo showing curvature grow linearly.
  Fenced off from the current physics-base initiative.
- CHANGELOG.md and a standing rule that all changes must be logged here.
- Per-ship physical state: mass and per-ship restitution (SHIP_MASS,
  SHIP_RESTITUTION in ship.h).
- Ship::resolveCollision(): Newtonian linear-momentum impulse for ship-on-ship
  collisions, reusing the existing SAT normal. Conserves momentum; conserves
  kinetic energy at e=1 and absorbs it as e drops toward 0.
- tests/collision_tests.cpp (CTest collision_unit): elastic, inelastic,
  glancing, separating-pair, and no-contact cases assert conservation.

### Changed
- resolveShipCollision() now applies the impulse model instead of flipping both
  ships' velocities and nudging ang (the old "bedoink").

### Planned
- Phase 2: spin from off-center hits (torque via the SAT contact point and lever
  arm). See plan-collision-physics.md.

## History (seeded from git log)

### 2026-10-05
- Next level: single-player mode with an offline adaptive AI opponent, a real
  explosion effect, and a start screen. (6729f10)

### 2026-10-01
- Dots count down. (0637ad0)

### 2026-09-30
- Keyboard fix. (8198de0)

### 2026-09-23
- Added screenshot. (dd1d6b7)
- Added Claude 5.5 note, released around that day. (c355d9a)
- Added comprehensive README with history and credits. (e6da168)
- Linux port: GLFW/Wayland, stb_image/stb_easy_font, procedural sound via
  miniaudio. (3e14b4d)
- Initial commit. (3a55bdd)
