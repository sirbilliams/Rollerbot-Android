# Deterministic whole-game routes

`tools/routes/run.sh` starts at level001, presses Space, and follows the actual exit
codes through the finale. The keys are zero-based **30 Hz simulation ticks**: each
`tick:scan,scan` entry replaces the held set; an empty entry releases every key.
Movement is automatic in these brains. Routes use 75/77 for turns, 29 for jumps,
and 57 on the title screen; Up (72) is unnecessary.

`rbbrain .game --route-chain tools/routes` runs the same chain directly.
`rbbrain .game level003 --keys @tools/routes/level003.keys --ticks 1800`
accepts a saved schedule, but an isolated project has different carried globals and
random/update state. **The proof is the whole-chain replay**, not isolated runs.
No player positions, collision masks, globals, lives, or scores are injected into
that replay. One VM carries globals and random/update state across every load.

The selector at exe 0x441cb0 maps positive exit codes to numbered projects;
0x4417e0 reads the corresponding multilev row for loading art. Those rows do not
specify the next project. Consequently the late game is 012 -> 015 -> 013 -> 014.
The finale returns to title with exit 1; replay stops before starting another game.

Scores and lives below are globals 0 and 220 at each normal exit. Ticks include
get-ready and completion animations. Lives 0 means the last life is still active;
the game-over value is -1. Fruit counts are the maximum observed global 201 before
exit brains clear it, compared with the initially enabled ID-64 objects.

| Order | Level | Exit | Fruit | Ticks | Score | Lives | Blocker |
|---:|---|---:|---:|---:|---:|---:|---|
| 1 | level003 | 4 | 22/22 | 1033 | 550 | 3 | None |
| 2 | level004 | 5 | 22/22 | 1571 | 1100 | 2 | None |
| 3 | level005 | 6 | 24/24 | 1603 | 1700 | 2 | None |
| 4 | level006 | 7 | 16/16 | 1095 | 2500 | 3 | None |
| 5 | level007 | 8 | 22/22 | 1533 | 3100 | 3 | None |
| 6 | level008 | 9 | 27/27 | 1776 | 3775 | 3 | None |
| 7 | level009 | 10 | 27/27 | 2069 | 4950 | 2 | None |
| 8 | level010 | 11 | 16/16 | 926 | 5700 | 2 | None |
| 9 | level011 | 12 | 32/32 | 2487 | 6550 | 1 | None |
| 10 | level012 | 15 | 32/32 | 2417 | 7400 | 0 | None |
| 11 | level015 | 13 | 16/16 | 979 | 8200 | 1 | None |
| 12 | level013 | 14 | 32/32 | 3504 | 9000 | 1 | None |
| 13 | level014 | 1 | 0/0 | 1553 | 9000 | 1 | None |

**Finished: 13/13. Final score: 9,000. Final lives: 1. No levels blocked.**

The title exits with code 3 after one tick. All bonus rounds (006, 010, 015) must
collect all 16 fruit: a bonus failure that happens to return the same exit code is
rejected. Every playable level must reach its expected normal exit with all fruit,
without any tick setting lives below zero. Compile errors, unknown commands,
timeouts, wrong exits, and missing key files fail the test.

The route generator in `tests/brain/full_routes.cpp` decodes the ground triangles
and collectible positions. A 2 m navigation grid provides paths, with water and
edge penalties. Bounded best-first search evaluates six-tick combinations of left,
right, straight, and jump in a separate BrainVM, including the actual moving-fruit
and enemy brains. Long trips use intermediate ground waypoints. Search retains a
safe continuation after each pickup as a fallback while beginning the next approach.
Level013 uses its lower-gravity jump selectively and visits the time ring when
the timer is low. Its timer brain adds 15 counter units through global 214; the
ring is reached using ordinary jump/turn input.
World snapshots are used only by this speculative VM; recorded inputs are then
played in the independent whole-game VM. Grid guidance and search estimates are
not completion evidence: the saved input replay supplies that evidence.

Relevant engine semantics are documented in `docs/brain.md`: ordered brain ticks
(0x410450), collision pass (0x44cad0), heading movement (0x40d040), and globals
across project loads (0x442220). No simulation bugs were proved or fixed;
`src/brain` is unchanged. These are completion routes, not damage-free speedruns.

Rebuild and verification, inside omtbuild:

```sh
timeout 900 cmake -S . -B build
timeout 900 cmake --build build -j8
timeout 900 ctest --test-dir build --output-on-failure
tools/routes/run.sh
```

`brain_full_game_routes` has a 900-second CTest timeout. It replays the entire
chain twice (420-second timeout each) and requires identical complete summaries
and final score 9,000. All 9 CTests passed in omtbuild; the full-game test took
3.67 seconds and the complete suite 4.53 seconds. A separate `run.sh` replay
confirmed the table above.
To regenerate from an existing prefix, use for example:

```sh
timeout 900 env RB_ROUTE_FROM=7 build/src/brain/tests/brain_full_routes \
  .game tools/routes --generate
```

The prefix is replayed from title, preserving its globals and random state before
search starts. Generation is deliberately separate from the CTest completion proof.
