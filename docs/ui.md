# UI lane reconstruction

Evidence: private `decomp/RECON.md`, `rb/raw/_free.c`, and, where decompiler
extents overlap, `rb/fallback/_free.asm`; English project records/brains and
retail font inspected read-only. All code here is original C++17. No game data
or decompiled bodies are included. Addresses below are executable virtual
addresses. **Provisional** labels identify reconstruction decisions, not facts
about the original game.

## Input

* `0x403ba0` creates DirectInput version 0x700, enumerates attached type-4
  joystick devices twice, and selects enumeration ordinal 1 then 2 (callback
  `0x403e20`). These are two joystick devices, not a keyboard/mouse pair.
  It queries the device interface, installs the joystick data format at
  `0x46eb38`, and sets foreground/exclusive cooperative flags 5. Failure disables
  that device independently. Acquire helpers: `0x403ef0`, `0x403f20`.
* `0x403f50` and `0x404060` Poll then GetDeviceState(0x50), i.e. DIJOYSTATE.
  They retain X/Y/Z, Rx/Ry/Rz, four POV values and all 32 high-bit button states
  packed into a word. Slider values are not copied. On 0x8007001e they attempt
  reacquisition; other read failures leave the last state unchanged.
  Device-1 globals start `0x57f1e8`, buttons `0x57f200`; device 2 starts
  `0x57f218`, buttons `0x57f230`. Acquisition/property helpers follow at
  `0x404170` onward; do not infer SDL axis calibration from raw DI values.
* Keyboard handling is separate Win32 state/message handling. `0x45e740`
  scans GetAsyncKeyState and uses MapVirtualKeyA; `0x45e820` pumps messages and
  waits for all keys to release during transitions. Project `keyboard(n)` uses
  PC scan-code numbers, with pressed represented as -1. The platform adapter
  must translate SDL keys to these numbers; SDL scancodes are different.

Observed game bindings (English level001 title brain and level003 onward player
brains; project evidence, not inferred from editor key shortcuts):

| Action | Keyboard scan code | Joystick |
|---|---|---|
| Turn left | keypad Left, 75 / 0x4b | joy1_x(-100,100) >= 90 |
| Turn right | keypad Right, 77 / 0x4d | joy1_x(-100,100) == -100 |
| Jump | left Control, 29 / 0x1d | joy_b(16) > 0 |
| Pause | P, 25 / 0x19 | none observed |
| Start from title | Space, 57 / 0x39 | joy_b(16) > 0 |
| Exit title / leave gameplay | Escape, 1 | none observed |

`joy_b(16)` is a mask test, so default fire is bit 4 (zero-based button 4),
not a claim that button number 16 is pressed. The reversed X mapping above is
intentional. No gameplay Y axis, POV, throttle or second-device use was found
in the inspected brains. No accelerate action: the player moves automatically.
Extended arrow-key equivalence is **provisional**; the platform may map both
keypad and extended arrows to the original action. Touch actions are explicit
booleans; there are no assumed screen dimensions. InputMap adds rising edges
for navigation while retaining held state for gameplay. On focus loss reset
InputMap and clear platform state (modern adaptation).

## Runtime screens versus editor controls

`0x423250` is the brain/list editing UI: element list selection, brain editing,
help and parameter editing, with page navigation. Its numeric list and editing
shortcuts are not Rollerbot's title menu. `0x42b530` is the engine device/options
dialog manipulating the quality word at `0x471a40`: device bits 0–1;
boolean display/filter/fog/dither/specular/texture/background flags at bits 2–8;
sound quality at bits 9–10 and sound mixing quality at bits 11–12. Low bits
select software RGB, hardware, software RAMP. These engine options exist in
the executable; reachability from the standalone game title is **unproven**.
Do not build an invented runtime options list from this editor dialog.

The game's opening/title project (`level001`) accepts Space/fire and loads the
next project; Escape exits. Playable projects have P pause and project-driven
Escape/reset handling, get-ready, hurry, time-up, game-over and completion
sprite sequences. They are not separate native menu dialogs. Menus exposes
Title/Playing/Paused and calls an IUiRuntime stub interface. **Provisional**:
startGame and returnToTitle deliberately delegate exact project selection,
reset and transition timing to the brain lane; resume on P is a host policy.
There is no invented level picker, high-score name entry, or editor screen.

## HUD and font

`0x439880` / `0x43de40` are engine object/brain UI anchors, not evidence of a
hard-coded game score bar. English level003 object records specify score
(84,8), high score (281,67), lives (86,66), and time-limit (304,22). The project
also supplies clock, lives, hi-score, power-up, fruit and other 2D sprite
backgrounds/overlays in a 640x480 logical layout. Hud accepts overlay handles
and adjustable positions; the renderer owns clipping, scaling and actual
asset lookup. The VM owns score/lives/time, visibility and sprite animation.
**Provisional**: the convenience Hud prints decimal values; exact credits
formatting, digit padding, per-project variants and animation require VM/scene
integration. HudState is a snapshot, not a second simulation.

`0x44b1c0` selects font.fnt / font8.fnt. `0x44aee0` reads a 0x333-byte header,
then two 0x400-byte tables, then a byte-stride times height bitmap. Font text
measurement/drawing anchors: `0x44a700` region and `0x44ab10` region. Inspected
font.fnt has version bytes 00 00 03 00, padded name starting at 4, byte widths
at 39 (256), LE16 horizontal bit offsets at 295 (256), LE16 height at 809,
byte stride at 811, spacing at 815, space advance at 817. Header's final
fields are packed, not naturally aligned C structs. Bitmap begins at 0xb33;
height 15, stride 183, total size 5612. Pixels are row-major, MSB-first.
The two intervening tables are unused by this monochrome decoder; their wider
color/translation semantics remain **unresolved**. Original loader overwrites
spacing with 2; Font retains the file value (2 for the inspected retail font).
Text is original byte-indexed glyph data, not UTF-8. Decoder validates glyph
extents and exact bitmap size, and rejects unsupported version/truncation.

## Score and persistence

Token records at `0x47334c`, `0x473370`, `0x473394` are score_add, score_set,
score. Their handler pointers are at the *end* of each 36-byte record:
`0x40c960`, `0x40c980`, `0x40c9a0` respectively (through jump aliases). They
add/set/read object field +0x5c. These commands alone do not define high-score
storage. English score-counter brains accumulate global 0; high-score brains
track the preceding marked score object and maximize global 100. Lives use
global 220. No player-name or ranked-table format was found.

`0x442220` project loading reads 0x2000 bytes into the global bank (read at
`0x4422e5`, filename globvars.dat at `0x47969c`). `0x41ae70` clears 0x800
32-bit slots. The corresponding project-transition writer writes that same
bank at `0x442873`; explicit save/load containers also embed it at offset
0x40008 (`0x442b60`, `0x442cbe`). Thus standalone globvars.dat is 2048 LE32
values, no header, and high score 100 is at byte 400. All slots are preserved,
including unknown/reserved slots; updating high score never lowers it.
`0x4125f0` recognizes globals_save/globals_load script syntax; these are generic
script persistence operations, not evidence of a separate highscore.dat.
The editor's temporary globvars path at `0x42c570` is a different context.
SaveStore uses the caller's writable directory, never the retail import tree.
Save timing remains the host/VM's responsibility; a visible maximum in RAM
is not automatically an on-disk update.

`0x41ca20` reads and `0x422260` writes 3drad.ini as **20 ordered text lines**,
not section/key INI syntax. Writer slots map to:

| Zero-based lines | Engine globals / meaning |
|---|---|
| 0–2 | 0x57f268 / 0x57f26c / 0x57f270; editor mode/flags, exact meanings unresolved |
| 3–6 | 0x57f440..0x57f44c; text-editor range/cursor state |
| 7–10 | 0x57f460..0x57f46c; editor state, exact meanings unresolved |
| 11–16 | 0x471ff4, 0x57f510, 0x57f304, 0x57f308, 0x471ff8, 0x57f514 |
| 17–18 | strings at 0x57f3a8 / 0x57f3ac (blank/version in inspected file) |
| 19 | 0x57f2c8 |

There is no observed gameplay key-binding table in this file. Preserve these
records rather than treating editor selection offsets as control mappings.
SaveStore retains arbitrary lines and emits CRLF. Caller supplies default
records if no INI exists; no fabricated field meanings are exposed.
`0x4060e0` reads quality.dat as four bytes; `0x4061a0` writes the LE32 quality
word; `0x42b530` edits it. Default 0x30f is the inspected retail configuration,
not a universally proven engine default. SaveStore preserves all bits.

Missing files leave defaults intact; malformed binary sizes throw before
changing state. Writes use a temporary sibling and rename per file. This
improves partial-write behavior but is not a three-file transaction and is
not safe for multiple simultaneous writers. No original corruption-recovery
behavior is claimed.

## Build and verification

Public interface: src/ui/Ui.h, namespace rb; link rb_ui. Platform translates
keys/gamepad to InputState and invokes Menus with its VM adapter. Asset lane
supplies font bytes and overlay names; scene implements IRenderer2D. No SDL,
GL, asset parser, VM or proprietary fixture dependency is needed to build.
Top-level integration: `add_subdirectory(src/ui)` (owned by platform lane).

Executed inside omtbuild (confirmed /run/.containerenv), all under timeout:

```sh
timeout 90 cmake -S src/ui -B build-ui
timeout 90 cmake --build build-ui -j8
timeout 30 ctest --test-dir build-ui --output-on-failure
timeout 30 build-ui/tests/rb_ui_test .game/font.fnt .shots/ui/font.ppm
timeout 15 python3 tests/ui/ppm_to_png.py .shots/ui/font.ppm .shots/ui/font.png
```

Checks cover synthetic font pixel orientation and malformed sizes, real font
decode/render, held versus edge input, touch/joystick, title-to-play and
pause/resume/return via VM stub, global-bank preservation, high-score maximum,
settings including blank lines, and rejection without partial load state.
The PNG was visually inspected: title, digits, upper/lowercase and punctuation
are legible. Private render output stays ignored under .shots. No GUI was run.
