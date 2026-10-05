# Copilot instructions for GravityShips2.o

## Changelog (required)

Whenever anything in this project changes — code, build, assets, docs, or these
instructions — record it in `CHANGELOG.md` as part of the same work. No change
is complete until it has a changelog entry. Add entries under `## [Unreleased]`
at the top, grouped as Added / Changed / Fixed / Removed, newest at the top.

## Build, run, test, and lint

This is a Linux-only C++17/CMake project. It requires OpenGL and GLFW 3 through
`pkg-config`; miniaudio, stb_image, and stb_easy_font are vendored in
`third_party/`.

```sh
# Configure (defaults to Release when CMAKE_BUILD_TYPE is unset)
cmake -S . -B build

# Build the only target
cmake --build build -j

# Run; the game opens fullscreen on the primary monitor
./build/gravityships
```

The post-build CMake command copies `assets/` beside the executable. Runtime
asset lookup is relative to `/proc/self/exe`, not the current working directory.

There is currently no automated test suite, individual test command, lint
target, or formatter configuration. Validation is a successful
`cmake --build build -j`; gameplay changes require running the game.

## Architecture

- `src/main.cpp` is the platform and game-orchestration layer: GLFW/OpenGL
  setup, keyboard polling, the fixed-order frame loop, stage progression,
  scoring, winner handling, texture loading, and 60 Hz pacing.
- `src/ship.{h,cpp}` contains both simulation and immediate-mode OpenGL
  rendering for ships, balls, pads, pellets, collision/landing state, fuel,
  ammo, and HUD text. `Ship::spaceShip` holds local vertices plus derived world
  locations; `updatePosition()` rebuilds world locations from the rotated local
  vertices and offset.
- The two `Ship` instances do not directly share live state. At the end of each
  frame, `main.cpp` calls `storeOtherShip()` to copy the peer's hull, pad, and
  ball into `otherShipsPosition`, `otherShipPad`, and `otherShipBall`. Collision
  and opponent-ball logic consume that snapshot on the next frame.
- `src/ai.{h,cpp}` splits the computer pilot into `steer()` (ideal controls)
  and `humanize()` (minimum burst/tap lengths and key gaps scaled by the
  adaptive difficulty); `fly()` composes them, so tests that call `fly()` see
  the same limited controls as the game.
- `src/vectorfont.{h,cpp}` draws the start screen's glowing stroke font in
  world units; the in-game HUD still uses `stb_easy_font` via `drawText()`.
- `src/sound.{h,cpp}` is a procedural synthesizer. The game thread sends
  one-shot `sfx::Event`s through a lock-free SPSC ring buffer and publishes
  continuous per-ship engine controls through relaxed atomics. The miniaudio
  callback owns voices and synthesis state at 48 kHz stereo. Audio startup
  failure is intentionally non-fatal and leaves the game silent.
- Rendering uses the fixed-function OpenGL 1.x compatibility API. World space
  is an aspect-adjusted orthographic range with horizontal wrapping at
  `-1.75..1.75`; HUD text is authored in the original 800x600 virtual
  coordinate system and scaled to the framebuffer.

## Repository-specific conventions

- Physics constants are per-frame, not time-scaled. Preserve the explicit 60 Hz
  limiter in `main.cpp`; changing it or introducing delta-time integration
  changes the tuned gameplay. Frame counts also define fire cooldown, ammo
  recharge, and spent-pellet lifetime.
- Preserve the frame-loop ordering. Input mutates ship state and draws thrust,
  then `updateShip()` handles bounds/physics/pads/scoring, pellets update, and
  peer snapshots refresh before ship-on-ship collision checks.
- A ship's six rendered vertices are not its collision hull. Collision and SAT
  code intentionally use vertices `0`, `2`, and `4` as the convex triangular
  hull. Landing checks intentionally use feet `2` and `4`.
- `ballhit` means the ship carries its own ball; `ballhitonce` gates moving the
  landing pad once per pickup. `landedOnPad` records a successful landing,
  while `onPad` is recomputed by `inBounds()` each frame and drives refueling
  and ammo reload.
- Stage values are score values (`STAGE1 == 0` through `STAGE5 == 4`).
  `resetBall()` and `resetLandingBase()` implement the score-dependent size and
  placement changes; `WIN_SCORE` is 5.
- Gameplay tuning values and geometry sizes live in `src/ship.h`. Keep values
  in the existing world units or frame counts, and update the README when
  player-visible controls or mechanics change.
- `sfx::play()` parameters are `(event, worldX, intensity/fraction, shipId)`.
  Use world X for stereo positioning and IDs `0`/`1` for per-ship pitch and
  cooldown behavior. Add new events consistently to `Event`, `kDur`,
  `render()`, and `kCooldown`; those arrays must remain in enum order.
- Do not allocate, lock, log, or call OpenGL/gameplay code in the miniaudio
  callback. The game thread is the sole producer of the audio queue and the
  callback is its sole consumer; a full queue deliberately drops an event.
- Vendored single-header implementation macros have exactly one owner:
  `STB_IMAGE_IMPLEMENTATION` in `main.cpp` and `MINIAUDIO_IMPLEMENTATION` in
  `sound.cpp`. `stb_easy_font.h` is header-only and included by `ship.cpp`.
- OpenGL matrix/state changes must be balanced. In particular, restore
  projection/modelview after HUD drawing, restore the default blend function and
  disable `GL_BLEND` after the additive explosion, and disable/unbind texture
  state after textured draws.
