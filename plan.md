# Single-player mode: offline adaptive opponent

## Problem and approach

Add a playable computer-controlled cyan ship while preserving the existing
same-keyboard two-player game. Start with an offline hierarchical opponent:
rule-based strategy selects goals, and a local controller issues normal ship
controls every physics frame. Keep the strategy boundary suitable for an
optional future Jev integration without adding networking infrastructure now.

Implementation is complete. The original plan is retained below; final
verification evidence is recorded at the end.

## Confirmed requirements

- Startup menu defaults to single-player and also offers existing two-player.
  Human controls yellow; the computer controls cyan.
- Display the current AI difficulty. Escape continues to quit everywhere.
  Restart the application for another match; no rematch or return-to-menu work.
- Start easy. Target normal when the AI trails by one point and hard when it
  trails by two or more. Ease back toward easy as the AI catches up.
- Include collecting, landing, refueling, dedicated attack, evasion, and blocking.
- Difficulty changes affect decisions and control quality, not physics, fuel,
  ammunition, scoring, collision tolerances, or other hidden advantages.
- Add lightweight CTest tests, scripted scenarios using actual ship physics,
  and manual gameplay validation.
- Jev, HTTP/JSON libraries, API keys, background network threads, billing,
  online play, and natural-language style configuration are deferred.

## Current code and constraints

- `CMakeLists.txt` builds one C++17 executable from `main.cpp`, `ship.cpp`, and
  `sound.cpp`. GLFW, OpenGL, and Threads are already linked; there is no test
  suite or formatter/lint target.
- `src/main.cpp:60-196` owns random ball placement, initial positioning, pad
  progression, refueling, and the mixed gameplay/rendering `updateShip()`.
- `src/main.cpp:302-428` owns input, sound publication, victory handling,
  rendering, ship updates, pellets, peer snapshots, collisions, and pacing.
  Ship 2's input block is the replacement point; do not replace its simulation.
- Preserve the 60 Hz frame limiter and per-frame constants. Preserve gameplay
  order, including input-time thrust drawing, `inBounds()`, `updatePosition()`,
  `rotateShip()`, pellet updates, peer snapshot refresh, and ship collisions.
- `src/ship.cpp:266-323` integrates `ang` into rotated local geometry every
  frame; rotation controls change `ang`. It acts as angular velocity, not an
  absolute orientation. Derive heading from the actual local nose vertex.
- `src/ship.cpp:604-635` computes thrust from local vertex 3, burns fuel, draws,
  and gives a landed ship a launch boost plus an extra position update.
  Prediction must account for this; do not substitute idealized lander physics.
- `src/ship.cpp:456-526` wraps horizontally, checks pickup, ground contact,
  landing, and pad collisions. Pickup requires distance below
  `ballSize + 0.02` and each velocity component strictly below `0.005`.
  Landing requires both feet within `0.001` of the deck, both velocity
  components below `0.001`, feet inside the pad, and the existing upright
  geometry checks. Reuse those rules rather than loosening them for the AI.
- Pad deck height is `landingPad[1].y`, not `landingLocation.y`. Use actual
  pad vertices and ship feet for approach targets. Stage 0 uses the broad
  initial ground-level pad; later stages shrink and eventually raise it.
- `landed`, `onPad`, and `landedOnPad` have different meanings. Treat them
  separately; the latter is not a reliable current-contact sensor.
- Firing changes translation and releases landed state; projectile hits and
  ship collisions can change spin. Recovery and attack must account for this.
- Control methods use OpenGL and audio. Never invoke them from a strategy
  worker; all action application stays on the game thread.
- A coupled terminal-score hazard exists: `updateShip()` calls `resetBall()`
  after reaching score 5, but `resetBall()` only initializes size for scores
  0-4. Fix this narrowly while implementing and testing terminal AI behavior.
- `.github/` is currently untracked and contains contributor instructions.
  Preserve it; do not stage or overwrite unrelated work.

## Implementation todos

### 1. Establish a small shared gameplay/test boundary

Files: `src/main.cpp`, new `src/gameplay.{h,cpp}`, `CMakeLists.txt`.

- Move existing initialization, ball/pad progression, refueling, and ship-update
  helpers into a small shared gameplay unit where needed by production and
  scenario tests. Preserve the current statements and rendering order; this is
  not a general engine or physics/rendering rewrite.
- Keep production random seeding at startup. Tests explicitly seed the same
  placement logic and record the seed on failure.
- Prevent generation of another ball at the winning score and further score
  increments beyond `WIN_SCORE`. Preserve the existing victory presentation
  and post-victory behavior otherwise.
- Wire source reuse in CMake so tests exercise production code, not copies of
  the physics or score rules.

### 2. Define observations, goals, and frame actions

Files: new `src/ai.{h,cpp}`, `src/ship.h`, `CMakeLists.txt`.

- Use simple value types for `AiObservation`, `AiDecision`, and `ShipControls`;
  no provider hierarchy, plugin system, or network-shaped framework.
- Copy position, velocity, derived heading, angular velocity, resources,
  cooldowns, score, carrier/contact flags, hull/feet, pads, balls, and relevant
  active projectiles from both ships at the beginning of the input phase.
  Include a frame identifier and avoid retaining live `Ship` references.
- Represent goals with a typed enum: collect ball, land on pad, refuel, attack,
  evade, and block. Include bounded aggression and a threat flag.
- Return only a mutually exclusive rotation direction, thrust, and fire.
  Apply actions using the existing rotation/thrust/fire methods, in keyboard
  order, with identical fuel costs, recoil, cooldowns, effects, and sounds.
- Keep strategy selection and flight control callable independently without
  an OpenGL context. Keep geometry/resource tuning in the existing world units
  and frame counts, with named AI-specific constants rather than magic values.

### 3. Build reliable per-frame flight and landing

Files: `src/ai.{h,cpp}`, focused tests.

- Implement shortest wrapped horizontal displacement for travel, while using
  the actual collision geometry and wrap behavior near contact.
- Use position/velocity feedback to choose bounded desired acceleration;
  compensate for gravity, brake before arrival, and regulate thrust with
  discrete per-frame decisions.
- Use heading and angular-velocity feedback to accelerate and brake rotation.
  Do not command spin from a heading error alone.
- Add explicit launch, transit, braking, overhead approach, alignment, descent,
  and settling phases. Stay above pad obstacles before a final descent; avoid
  approaching a raised pad through its underside.
- Target slow ball interception and upright two-foot touchdown with margins
  inside the production thresholds. Use low terminal descent speed to avoid
  stepping over the narrow contact window.
- Recognize pickup and relocated pads on the next observation, and immediately
  invalidate obsolete targets. Handle the wide initial pad separately from
  smaller raised pads using their geometry, not hard-coded stage coordinates.
- Reserve fuel for recovery and returning to the own pad. Wait there for
  refueling/reloading with separate enter/exit thresholds to prevent thrashing.
- Recover from impacts, unexpected spin, overshoot, and stalled progress using
  bounded replanning. At empty fuel, issue no impossible control assumptions
  and do not grant a rescue teleport or refill.
- Keep the safety/landing controller reliable at all difficulty levels;
  easy mode is not implemented by skipping physics-frame corrections.

### 4. Add strategic combat and adaptive difficulty

Files: `src/ai.{h,cpp}`, focused tests.

- Reconsider strategy every 20 simulation frames, with immediate invalidation
  for completed/impossible goals and safety-critical state changes.
- Priority order: imminent collision/impact recovery, fuel survival, viable
  delivery, then collection or bounded combat opportunities. Carrying a ball
  and low fuel reduce willingness to abandon productive play.
- Attack: select a feasible firing approach, lead using relative movement and
  pellet gravity, respect ammunition/cooldown, and account for recoil. Do not
  fire when it would spoil a final landing or essential refueling.
- Evade: predict nearby ship/projectile threats from observed trajectories,
  select a safe lateral/vertical escape, and resume productive goals afterward.
- Block: intercept the opponent's observed ball or delivery approach from a
  safe standoff position, without assuming future random placements. Include
  a fixed commitment limit and retreat/replan rules to avoid endless camping.
- Use goal minimum dwell times, progress checks, and explicit completion/
  abandonment conditions so attack, evade, and block do not oscillate or
  permanently starve scoring.
- Compute target difficulty from `humanScore - aiScore`: easy at <= 0, normal
  at 1, hard at >= 2. Start each match at easy.
- Smooth the internal difficulty toward its target over 180 frames for a
  full easy-to-hard transition, allowing equally gradual decreases. Show the
  effective level, including a transition indicator when it differs from target.
- Tune difficulty through strategic commitment, safe cruise speed, firing
  precision/frequency, and combat opportunity selection. Keep terminal
  landing tolerances and safety overrides unchanged.
- Bound all prediction and evaluation loops. Strategy and control must never
  sleep, wait on I/O, allocate unbounded work, or alter simulation pacing.

### 5. Integrate startup menu and mode-aware controls

Files: `src/main.cpp`, `src/ship.h`, existing `drawText()` in `src/ship.cpp`.

- Add a small startup state: Up/Down selects single-player or two-player,
  Enter starts, and Escape quits. Use key-press edges so held keys do not
  repeatedly change selections.
- Reuse existing text rendering through a proper declaration; do not add a UI
  dependency or duplicate the font implementation.
- Do not advance physics, consume fuel, or publish active engines in the menu.
  Initialize the selected match and play the start cue on transition to play;
  reset the frame deadline to avoid carrying menu time into gameplay.
- Preserve Player 1 controls. In two-player mode preserve Player 2 controls
  exactly; in single-player ignore those keys and apply AI actions instead.
- Capture the AI observation before either player's current-frame actions,
  without changing the existing peer snapshots used by collisions.
- Display `CPU` and effective difficulty near cyan status without overlapping
  scores/resources. Explain yellow controls and mode choices in the menu.
- Stop AI decisions and actions once either ship wins. Maintain existing
  sound engine reset/publication and winner effects.

### 6. Add tests and measurable scenario gates

Files: new `tests/ai_tests.cpp`, `tests/ai_scenarios.cpp`, `CMakeLists.txt`.

- Enable CTest through standard `BUILD_TESTING`; use small self-contained C++
  test executables and explicit failure reporting, with no new test framework.
- Pure tests need no display: wrapped displacement, heading/sign conventions,
  spin braking, goal changes, reserve hysteresis, difficulty increase/decrease
  and clamping, combat expiry, threat response, and legal action output.
- Integration scenarios use a hidden GLFW/OpenGL context and production
  `Ship`/gameplay helpers, retaining the actual control and update order.
  Do not stub out physics, landing checks, or resource accounting.
- Separate graphics-dependent tests from display-free tests with CTest labels.
  A missing display/context must be reported explicitly, not treated as a pass.
  Use an available Xvfb/software-rendering environment when needed; document
  this as test infrastructure, not a new game runtime dependency.
- Exercise every score stage independently across 20 fixed seeds at easy,
  normal, and hard. With a non-interfering opponent, require at least 18/20
  pickup-and-delivery completions per stage/level within 18,000 frames each.
  Require success on curated narrow-pad and wrapping scenarios as well.
- Add deterministic cases for moving targets, initial-pad launch, relocated
  pads, braking, spin recovery, low fuel/refueling, empty fuel, projectile
  avoidance, attack aiming/recoil, blocking exit, and ammo reload/cooldown.
- Assert pickup and landing against actual production predicates and score/
  contact transitions, not merely distance to a target.
- Run seeded full-match scenarios through the winning score and check that
  scores remain bounded and the AI emits no further actions after victory.
- Record seed, stage, difficulty, frame, goal, resources, and failure reason
  for reproducibility. Use bounded output rather than per-frame logging.
- Measure AI execution separately from rendering and frame sleep; require
  p99 strategy-plus-control cost below 1 ms per frame on the validation
  machine. Report hardware/build details and tune if this gate fails.
- Tune the controller against these gates before considering it playable;
  a successful compile alone is insufficient.

### 7. Document and perform end-to-end verification

Files: `README.md`.

- Document single-player startup, controls, CPU colors/label, adaptive score-
  deficit behavior, combat, unchanged rules, and restart/quit behavior.
- Explain that this version is fully offline and requires no API key or cost.
  Add source layout and exact CTest/scenario commands and display requirements.
- Configure/build with the existing CMake commands; run both CTest groups.
- Manually check fullscreen menu navigation, held keys, Escape, both modes,
  audio continuity, HUD layout, pickup/delivery/refueling at narrow pads,
  difficulty easing/increasing, combat behavior, and both victory outcomes.
- Verify that the two-player controls, frame order, tuned mechanics, and
  asset lookup still behave as before. Report unavailable interactive or
  graphical checks honestly rather than claiming validation.

## Dependencies and execution order

1. Shared gameplay/test boundary.
2. Observation/action contract (can be implemented independently of step 1).
3. Flight controller, using step 2.
4. Adaptive strategy and combat, using steps 2 and 3.
5. Menu and input integration, using steps 1-4.
6. Automated validation infrastructure begins with steps 1-2; final scenario
   acceptance depends on steps 3-5.
7. Documentation and end-to-end verification depend on steps 5-6.

## Deferred Jev extension

Keep the only extension seam at `observation -> decision`; the per-frame flight
controller remains authoritative. Do not implement a fake Jev provider now.
A separate future plan should verify the current API schema, typed question
support, access terms, latency, and pricing rather than treating the estimates
in the original proposal as verified facts.

That extension would need opt-in credentials, bounded asynchronous requests,
one in-flight request or equivalent backpressure, timeouts, stale/out-of-order
result rejection, semantic validation of goals against current state, visible
failure status, and continued offline strategy when disabled or unavailable.
The worker must receive copies only and must not access OpenGL, live ships, or
the single-producer audio queue. No network outcome may block the frame loop.

## Risks and scope discipline

The core risk is reliable flight under the existing angular momentum,
launch/contact quirks, and narrow landing window, not goal selection. Establish
repeatable scoring before tuning aggression. Combat is required, but must not
turn the opponent into an aimless attacker.

Do not use unlimited fuel, teleports, direct velocity edits, relaxed collision
rules, or a rewritten physics model to meet scenario targets. If actual gameplay
exposes a blocking pre-existing defect beyond the terminal-score issue, isolate
and explain it before broadening the work; do not silently change two-player
mechanics. Avoid unrelated cleanup, new persistence, difficulty menus, general
AI frameworks, and new runtime dependencies.

## Implementation and verification result

- Implemented the offline adaptive opponent, shared gameplay helpers, startup
  menu, CPU HUD, and documentation. Jev remains deferred.
- Release build and both CTest groups pass. All 300 seeded stage/difficulty
  deliveries complete, along with five parked-opponent matches, three
  live-idle-opponent matches, and curated recovery/resource/combat scenarios.
- Debug builds with undefined-behavior sanitizer also pass both CTest groups.
  Release AI p99 was approximately 2 microseconds on an Intel Xeon E5-1650 v2,
  using GCC 16.2.1; well below the 1 ms gate.
- Exercised the real fullscreen application using targeted compositor keyboard
  events. Inspected menu, two-player, and single-player screenshots at 2048x1152.
  Verified held Down selects once, quick Enter starts, both players' thrust/
  firing and opposite rotation inputs work, cyan keyboard input is ignored in
  single-player, CPU labeling does not overlap resource text, and Escape exits.
- The UI check exposed missed very short menu key taps with polling. Menu
  actions now use GLFW press callbacks (not repeat events), and the quick-tap
  and held-key checks pass after rebuilding.
- Verified the game's own PipeWire audio output node is running, with no audio
  startup errors. This checks audio integration, not a subjective listening
  assessment of sound quality.
- Closed all test game instances. Screenshots are retained in session `files/`;
  no unrelated repository changes were overwritten or committed.
