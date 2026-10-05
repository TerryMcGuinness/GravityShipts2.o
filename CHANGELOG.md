# Changelog

All notable changes to GravityShips2.o are recorded here.

This project is a fork of the original GravityShips (Terrence McGuinness,
originally authored ~2015), rebuilt as a Linux-native C++17/CMake game.

The format is loosely based on Keep a Changelog (https://keepachangelog.com/).
Newest entries go at the top. Every change to the project must be recorded here
as part of the same work (see .github/copilot-instructions.md).

## [Unreleased]

### Added
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
