# Plan: Newtonian ship-on-ship collision

## Goal

Replace the current binary ship-on-ship collision (detect overlap, play a
Clang, nudge apart) with a real rigid-body impulse model grounded in Newtonian
mechanics. Two ships with mass that collide should exchange momentum like
physical objects, with a restitution coefficient spanning perfectly elastic
(kinetic energy conserved) to inelastic (energy absorbed into deformation /
damage state). No more "bedoink."

## What already exists (reuse, do not reinvent)

- `convexMTV()` in `ship.cpp`: full SAT over two convex polygons, returns a
  minimum translation vector and an implied collision normal. This is the
  contact normal and penetration depth source.
- Pellet restitution in `ship.cpp` (~L134): reflect relative velocity about a
  unit normal with `v' = v_body + (v - v_body) - (1+e)((v - v_body)·n) n`.
  This is the 1D impulse pattern; generalize it to two moving masses.
- `Ship::velocity` (Point2D): per-ship linear velocity, already integrated each
  frame in `updatePosition()`.
- `storeOtherShip()` / peer snapshot: `otherShipsPosition`, `otherShipPad`,
  `otherShipBall` refreshed each frame before collision checks.
- Collision hull convention: vertices 0, 2, 4 form the convex triangular hull
  (per copilot-instructions). Use that hull, not all six rendered vertices.

## What to add

1. Physical state on the ship
   - `float mass` (named constant; both ships equal mass -- decided).
   - `float restitution` (coefficient e, 0..1). Per-ship state so damage can
     lower it later; both start at the same value -- decided.
   - Phase 2: moment of inertia + angular velocity for spin (see below).

2. A collision resolver with a clean boundary
   - Signature in spirit: takes both ships' states (mass, velocity, hull,
     center), computes post-collision velocities, writes them back.
   - Steps: detect contact via `convexMTV` on the two triangular hulls ->
     get normal n and penetration; positional correction to separate them
     along n (split by mass); compute relative velocity along n; if they are
     approaching, apply impulse j = -(1+e)(v_rel·n) / (1/m_a + 1/m_b);
     v_a += (j/m_a) n, v_b -= (j/m_b) n.
   - Keep it a pure-ish function callable without OpenGL (testable), matching
     how ai.cpp separates logic from rendering.

3. Wire-in
   - Replace the body of `ifShipsColide()` (or add a resolver it calls) so the
     Clang still plays on genuine impact, keyed to impact strength (|v_rel·n|)
     for volume, mirroring the existing Thud intensity pattern.
   - Preserve frame-loop ordering: collision stays where it is now, after peer
     snapshot refresh.

## Constraints / do-not-break

- Physics constants are per-frame, not time-scaled; keep the 60 Hz limiter and
  express new constants in the same world units. (copilot-instructions)
- Do not alter two-player mechanics silently; AI consumes the same physics.
- Momentum and (for e=1) kinetic energy must be conserved -- assert in tests.
- Keep landing/pad/pellet collision paths unchanged; this is ship-on-ship only.

## Tests (CTest, matching existing ai_tests pattern)

- Head-on equal masses, e=1: velocities swap; momentum + KE conserved.
- Equal masses, e=0: common final velocity; momentum conserved, KE drops.
- Unequal masses: check momentum conservation and correct velocity split.
- Glancing hit: only the normal component changes; tangential untouched.
- Separating pair already apart: no impulse applied (no sticking).

## Decisions

- Both ships equal mass.
- Restitution is conceptually a function of ship damage level. Stored
  per-ship so damage can lower it later; both start equal.
- Spin from off-center hits is wanted, built as Phase 2 on top of a tested
  linear base (see Phase 2).

## Phase 1 -- linear momentum (do first, get solid + tested)

1. [branch done] `physics-collision`
2. [this file] plan
3. Add mass + per-ship restitution state and constants to `ship.h`.
4. Implement linear resolver (new function; reuse convexMTV + impulse math).
5. Wire into ifShipsColide / frame loop; impact-scaled Clang.
6. Add CTest cases; assert momentum conserved (and KE at e=1).
7. Update README + CHANGELOG as part of the same work.

## Phase 2 -- spin from off-center hits (layer on the tested base)

Rationale: once spin enters, linear impulse and angular response are coupled
and solved together; moment of inertia and feel need tuning so ships do not
over-spin. Build only after Phase 1 conserves momentum and passes tests.

1. Add moment of inertia + angular velocity state; `ang` already exists.
2. Use the SAT contact point; r = contact - center for each ship.
3. Impulse through the contact point: torque = r x j, scaled by 1/I; longer
   lever arm (nose hit) -> more spin, center hit -> pure shove.
4. Use the coupled impulse formula (normal + rotational terms together).
5. Tests: off-center hit induces spin; center hit induces none; angular +
   linear momentum both conserved; cap/tune so spin stays believable.
6. Update README + CHANGELOG as part of the same work.
