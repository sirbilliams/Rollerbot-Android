# Retail level sweep

Each project runs independently for 5400 ticks (180 seconds at 30 Hz) with idle,
hold forward (scan code 72), and forward plus alternating 180-tick turn segments
(scan codes 75/77). Each process has a 120-second wall timeout. Exits are recorded
by code and cleared before the next tick, retaining the current project for coverage;
these counts are exit requests, not proof of completed routes. Lives lost sums positive
decreases of global 220. Counts aggregate all three patterns. NaN checks cover every
element's scene transform each tick; VM state itself uses integer fixed point.
Runtime errors include exceptions, signals, timeout, and missing completion records.
Retail movement is automatic; forward holds the Up input (72). Bare `rot=>80`
and `rot=<30` in level004 are accepted as discarded comparisons, with unit coverage.
This compatibility policy preserves the variable; the exe compiler's malformed-source
behaviour remains unverified (statement/expression paths 0x4125f0/0x414880).
No new command handlers were required: no UNIMPLEMENTED calls were reached.
Logs: `.shots/sweep/logs/`. Reproduce: `tools/sweep/sweep.sh` after building.

| Level | Loads | Ticks idle/forward/turns | Compile errors | UNIMPLEMENTED hits | Runtime errors/NaNs | Exit requests (code: count) | Lives lost |
|---|---:|---|---:|---|---:|---|---:|
| level001 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | — | 12 |
| level003 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 12 |
| level004 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level005 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level006 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 7: 15276 | 0 |
| level007 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level008 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level009 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level010 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 11: 15273 | 0 |
| level011 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level012 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level013 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 3 | 9 |
| level014 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 1: 11544 | 0 |
| level015 | 3/3 | 5400/5400/5400 | 0 | — | 0/0 | 13: 14867 | 0 |

Levels clean: 14/14.

Captures: `tools/sweep/shots.sh` uses the real `build/src/platform/rollerbot`,
RB_DATA=.game, RB_LEVEL per project, RB_FRAMES=150, RB_SHOTS at frame 150,
SDL_VIDEO_X11_FORCE_EGL=1 and Xvfb. Images: `.shots/sweep/levelNNN.png`;
contact sheet: `.shots/sweep/sheet.png`. Build and all 8 CTests passed in omtbuild.
