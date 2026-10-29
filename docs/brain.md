# Brain / project language (lane brain)

Status: 340 distinct commands found in the exe table; 87 implemented in `src/brain/Brain.cpp`
(all 79 distinct commands used by the 14 retail projects except `dbg_color_set`, which is a typo/no-op and absent from the table).
Everything marked GUESS is inferred from names/values, not confirmed by the exe.

## 1. What a project is
- `.prj` (DWORD-wrapped, see formats.md): line 1 `;v1.0`, line 2 object count N, then N pipe records (50 fields), then
  one brain section per object: `@<index>` followed by the brain source text (plain BASIC-like text, CRLF).
  The 3D Rad editor compiles that text into x86 machine code at load time (compiler `0x4125f0`, huge string-based
  function; error strings at 0x471318..0x4718c8: "Unknown symbol", "If...endif nesting error!", "Too many variables defined!").
  Our port does not run machine code; it interprets the same text. Each brain is called once per frame.
- `.col`: 65,536 bytes; semantics unresolved (collision pairs are set by `collision_map_set`, not read from the .col as far as seen).
- Object record fields used (0-based, from value statistics across all 14 levels; GUESS where noted):
  0 name, 1 model (`./HASH.dat`), 2 sound, 4-6 position in mt (x,y,z), 10-12 rotation in degrees (x,y,z),
  30 collision id bits (`+0x60`; player 8, aliens 2, bosses 4, apples 64, rings 128), 31 unused-looking partner mask,
  32 initial flags (bit0 = on), 33 paused (1 = brain never runs, GUESS; ground scenery), 34 compiled variable table
  (`count/type/value/slot...`, ignored; initial values come from the `var` lines), 35 object type (2 = 3D object, 5 = sprite/overlay,
  8 = counter, 10/11 = model/scenery; GUESS), 36 viewport `x0/y0/x1/y1` for sprites/cameras, 41-43 scale (1 or 1000).
- Element struct layout from handlers (stride 0x400, array base 0x57efb0, count 0x57f084): +0x0c pos xyz, +0x18 rot xyz,
  +0x24 velocity xyz, +0x30 spin xyz, +0x54 compiled brain entry, +0x58 flags, +0x5c credits/score, +0x60 id, +0x64 collided ids,
  +0x68 radius, +0x70 sound handle, +0x7c.. scale, +0xac run counter.
  Flags: 1 on, 2 paused, 8 child, 0x10 no-hide-hook, 0x20 translucent, 0x40 visible, 0x80 collisions on, 0x100 use +0x78 radius.

## 2. Language
- One statement per line; `;` starts a comment; case-insensitive identifiers; indentation irrelevant.
- Declarations: `var name=expr`, `userInt name=expr`, `userMet name=expr` (editor-exposed variables; same semantics).
  Initial value is evaluated at load only; values persist between frames (this is how brains keep state, e.g. `start`).
- `if <cond>` / `else` / `endif` (nesting checked, messages 0x4714b0..0x471550). `goto`, labels (max 16 chars), `run(`, `run_task(`,
  `play(`, `play_task(`, `globals_save(`, `globals_load(` exist as compiler keywords (strings 0x47134c..0x4713f8,
  0x471434) but no retail brain uses them; NOT implemented.
- Expressions: 32-bit ints; `+ - * /` (divide by 0 gives 0, GUESS), unary `-`, brackets `[ ]` and `( )`, `and or xor` (bitwise;
  comparisons yield -1/0 so they act as logic), comparisons `= <> < > =< <= => >=` (exe operator set 0x471820 `/|*|-|+|=|!|<|>`).
  Precedence (GUESS, standard): `or/xor` < `and` < comparison < `+ -` < `* /`. A bare number right after an operand
  (`collision(element,2)0`) is treated as `<>` (GUESS).
- Number literals take unit suffixes (table 0x471744, converters from thousandths): `mt` = 512,000 raw (0x426ac0, v<<9),
  `dg` = 512 raw per degree (0x426b10), `ms` = 17.067 raw (0x426b80, 512000/30 per tick), `rm` = 0.1024 raw (0x426bf0).
  So positions are 1/512000 m, angles 184,320 per turn, velocity is raw units per frame.
- `element` = this element (reference value); `element#N` / `element#-N` = element N places after/before it in the list
  (ref arithmetic `ref + N*0x400`, `element_index` 0x407650). References are first-class ints stored in variables.
- Argument passing in the exe: eax,ebx,ecx,edx,esi in order (register names table 0x471664). Function-like commands return in eax.

## 3. Execution model
- Frame (transition/run loop 0x442795..0x4427ae and 0x410450):
  1. message pump 0x45f7b0 (input/timer, not simulation);
  2. 0x42a110 per-sprite animation counters (not simulated);
  3. collision pass 0x44cad0 (see below);
  4. input update 0x403f50/0x404060;
  5. 0x410450: clear the child flag (8) on all elements; then for each element in list order, if `on` and not `paused`
     call its brain (+0x54), increment run counter +0xac; finally decrement 0x57f354 if > 0, increment tick counter 0x57f1dc
     (drives `every_tics`).
- Order = element list order, everything sequential and immediate; there is no physics other than what brains call
  (`velocity_update`, `spin_update`, `friction_*`, ...). Frame pacing is the platform's: we use 30 ticks/s (GUESS from the `ms`/`rm` constants).
- Collision pass 0x44cad0: pairs are registered by `collision_map_set(a,b,color)` (0x41af10, color <= 0xb; 16-byte records at 0x57efc4).
  Each frame the `+0x64` masks of pair members are cleared, then for every pair where both elements have flags 0x81 and
  |dx|,|dy|,|dz| (>>9) < r_a + r_b and the true distance <= r_a + r_b, `a.collided |= b.id` and vice versa and the pair colour is
  remembered for `collision_color`. `collision(e, mask)` = `e.collided & mask`. Radius +0x68 is the model bound (units 1/1000 mt)
  recomputed on `scale_set`; we derive it from the mesh vertex distance (GUESS).
- Level start: 0x442220 loads the project, restores globals from `globvars.dat` (2048 int32), runs transition updates; `exit(n)` stores
  n in 0x57f2dc and ends the run loop. Retail brains call `exit(<level number>)`, `exit(level)` and `exit(-3)`; the next project
  is picked through `multilev.ini`/0x4417e0 (rows of `loading-screen|default` PCX names, not level numbers); exact mapping UNRESOLVED
  (`BrainVM::world().exitCode` hands the value to the caller).
- Globals: `global(n)`, `global_set`, `global_add`, index masked with 0x7ff (0x40a010..0x40a050). Used as the game state bus
  (220 lives, 201 fruit collected, 203 speed-up points, 205 power, 206/207 get-ready, 210 game over, 212 sound select, 213 time limit).
  `global_set` also feeds a network mirror (0x57f540) that we ignore. Persistence (`globals_save/globals_load`, `globvars.dat`) is in 0x4125f0/0x442220, NOT implemented.
- Score: `score(e)`, `score_set(e,v)`, `score_add(e,v)` are aliases of `credits*` (the +0x5c field; same handlers 0x40c9a0/0x40c980/0x40c960).
  The visible score digits are a `Score Counter` element (type 8) whose own brain (user vars `pts188` ... = points per collision colour)
  presumably reads the pair colours; the high-score file format is still unidentified.

## 4. Command semantics implemented
Positions/angles/velocities are raw ints (see units). `e` is an element reference.
- Accessors: `position_x/y/z(e)`, `position_?_set(e,v)`, `position_y_add`, `position_copy(from,to)`, `rotation_x/y/z(e)` (normalised mod 0x2cfff, note the exe quirk, 0x44c360),
  `rotation_?_set/_add`, `velocity_x/y/z_set`, `velocity_y_add`, `spin_y_set`, `credits/score` family.
- Kinematics: `velocity_update(e)` pos += vel (0x40a5e0); `spin_update(e)` rot += spin (0x40a900);
  `speed_xz_set(e,s)` vel.x = sin(roty)*s>>7, vel.z = cos(roty)*s>>7 using the byte table sinus.bin (row=angle>>9, bytes +1 and +0x5aff; 0x40d040);
  `thrust_xz` adds instead (0x40b570); `speed_set(e,s)` rotates (0,0,s) by the element's rotations (x, then z, then y; 0x40d090);
  `friction_y/friction_xz(e,f)` v = trunc(v*f/512) (0x40ba90/0x40b990); `attract_xz(e,t,f)` adds f toward t scaled by max(|dx|,|dz|) (0x4098d0).
- Hierarchy: `join(a,b)` copies pos,rot,vel,spin a->b (0x40b280); `join_pos` copies pos and velocity (0x40b2f0); `child_pos(e,parent,x,y,z)` places e at parent plus the offset
  rotated through the parent's rotation using 15-bit sinusbig.bin (0x409af0); `child_rot` copies rotation and sets the child flag (0x409cb0);
  `follow(e,target,y,z,k)` eases e toward target + offset(0,y,z rotated by target yaw): pos += ((goal-pos)>>9)*k<<2 (0x40b630);
  `track(a,b)` sets a's pitch and yaw to face b, `track_yr` only yaw (0x409df0/0x409e80; the atan conventions are GUESSES).
- Flags: `on/off/show/hide/collisions_on/collisions_off/translucent(e,0|1)/state(e,mask)` (0x40c020, 0x40bff0, 0x40c090, 0x40c0b0, 0x40c050, 0x40c070, 0x451ed0, 0x40cac0).
- Queries: `collision`, `collision_color(n)`, `collision_map_set(a,b,color)`, `nearest_element(e,idmask)` (nearest `on` element with matching id bits, 0 if none; 0x407330),
  `distance(a,b)` (0x44c3a0), `element_index(ref,n)`, `element_id(e)`, `first_element()`, `execute(e)` (runs e's brain now, depth-limited),
  `ground_set(e)` + `ground_level(e)` (non-zero over the ground mesh, 0 over water/void; the exe queries the ground mesh with 0x40189d/0x40228e; ours is an IScene hook, GUESS),
  `sin_512/cos_512(angle)` = round(sin(a/29335)*512) (0x4077b0), `random(lo,hi)` (0x40c670/0x45f540/0x4608b0: bytes of random.bin plus the frame counter; GUESS that
  tick count replaces the counter), `every_tics(period,until)` true when tick < until and tick & (period-1) == 0 (0x40b5c0).
- Input: `keyboard(scancode)` -1 when held (scancode->VK->GetAsyncKeyState, 0x40b370; 29 = Ctrl, 75/77 = left/right arrows, 57 = space), `joy1_x`, `joy_b`, `any_input`.
- Audio/scene: `play_sound(e)` (plays e's own sound, requires sound handle), `stop_sound`, `sound_rate_set`, `listener(e)`, `light_color_set`, `light_intensity_set`,
  `3dbg_color_set` (background colour), `scale_set(e,x,y,z)`, `flat_scale_set(e,x,y)`; all forwarded through `IScene/IAudio`.
- Flow: `exit(n)`, `pause()` (no-op, original blocks), `global*`.
- Not implemented (no retail use in the ones checked): the remaining table entries below, race/network/checkpoint/signal families, animate, boing_*, etc.
  They are reported by `BrainVM::unknownCommands()` and the `UNIMPLEMENTED` trace event.

## 5. Command table (exe 0x472008..0x474f90, 36-byte records: `fn|name|nargs|flag|` string + handler thunk at +0x20)
`fn` = returns a value, `stmt` = statement. "Handler" is the resolved implementation address.

| name | kind | args | handler | implemented |
|---|---|---|---|---|
| 3dbg_color_set | stmt | 3 | 0x406d60 | yes |
| above_ground_level | fn | 1 | 0x409390 | yes |
| absolute_value | fn | 1 | 0x407670 |  |
| abs | fn | 1 | 0x407670 |  |
| agl | fn | 1 | 0x409390 | yes |
| aglevel | fn | 1 | 0x409390 | yes |
| angle | fn | 2 | 0x409f50 |  |
| animate | stmt | 4 | 0x409650 |  |
| animation_frame | fn | 1 | 0x409770 |  |
| animation_frame_set | stmt | 2 | 0x409790 |  |
| any_input | fn | 0 | 0x40b320 | yes |
| attract | stmt | 3 | 0x409800 |  |
| attract_xz | stmt | 3 | 0x4098d0 | yes |
| background_color | fn | 1 | 0x4071b0 |  |
| between | fn | 3 | 0x40c600 |  |
| boing_x | stmt | 1 | 0x409a90 |  |
| boing_y | stmt | 1 | 0x409ab0 |  |
| boing_z | stmt | 1 | 0x409ad0 |  |
| camera_fov | fn | 1 | 0x4071e0 |  |
| child_pos | stmt | 5 | 0x409af0 | yes |
| child_rot | stmt | 5 | 0x409cb0 | yes |
| collision | fn | 2 | 0x40be10 | yes |
| collisions_off | stmt | 1 | 0x40c070 | yes |
| collisions_on | stmt | 1 | 0x40c050 | yes |
| collision_color | fn | 1 | 0x40bdd0 | yes |
| collision_list | fn | 3 | 0x40be30 |  |
| collision_map | fn | 2 | 0x41aed0 |  |
| collision_map_set | fn | 3 | 0x41af10 | yes |
| collision_range | fn | 3 | 0x40be80 |  |
| counter_picked | stmt | 1 | 0x417d50 |  |
| credits_add | stmt | 2 | 0x40c960 | yes |
| credits_set | stmt | 2 | 0x40c980 | yes |
| credits | fn | 1 | 0x40c9a0 | yes |
| debug | fn | 1 | 0x406790 |  |
| dimension_abs | fn | 2 | 0x407690 |  |
| direction_xz | fn | 2 | 0x409e80 |  |
| distance_xz | fn | 2 | 0x40bf40 |  |
| distance | fn | 2 | 0x44c3a0 | yes |
| divide | fn | 2 | 0x407720 |  |
| drag | stmt | 2 | 0x40b750 |  |
| drag_xz | stmt | 2 | 0x40b820 |  |
| drag_y | stmt | 2 | 0x40b8b0 |  |
| element_index | fn | 2 | 0x407650 | yes |
| engine_get | fn | 0 | 0x406920 |  |
| engine_set | stmt | 1 | 0x406940 |  |
| every_tics | fn | 2 | 0x40b5c0 | yes |
| exemode | stmt | 1 | 0x4066b0 |  |
| exit | stmt | 1 | 0x40b5f0 | yes |
| factor_512 | fn | 2 | 0x40b610 |  |
| first_element | fn | 0 | 0x40cb30 | yes |
| flash | stmt | 1 | 0x4076c0 |  |
| flat_scale_set | stmt | 3 | 0x4067d0 | yes |
| flat_scale_x | stmt | 1 | 0x406850 |  |
| flat_scale_y | stmt | 1 | 0x406870 |  |
| follow | stmt | 5 | 0x40b630 | yes |
| frame_refresh | stmt | 1 | 0x4075e0 |  |
| frame_refresh_all | stmt | 0 | 0x407600 |  |
| friction | stmt | 2 | 0x40b900 |  |
| friction_xz | stmt | 2 | 0x40b990 | yes |
| friction_y | stmt | 2 | 0x40ba90 | yes |
| global | fn | 1 | 0x40a010 | yes |
| global_add | stmt | 2 | 0x40a030 | yes |
| global_set | stmt | 2 | 0x40a050 | yes |
| gravity | stmt | 4 | 0x40a0e0 |  |
| ground_level | fn | 1 | 0x409360 | yes |
| ground_slope | fn | 1 | 0x408760 |  |
| hide | fn | 1 | 0x40c0b0 | yes |
| inside_area_xz | fn | 5 | 0x40b210 |  |
| inside_volume | fn | 1 | 0x41aa40 |  |
| interpolate | fn | 5 | 0x40b250 |  |
| joy_b | fn | 1 | 0x40a100 | yes |
| joy1_b | fn | 1 | 0x40a100 |  |
| joy1_x | fn | 2 | 0x40a140 | yes |
| joy1_y | fn | 2 | 0x40a190 |  |
| joy2_b | fn | 1 | 0x40a120 |  |
| joy2_x | fn | 2 | 0x40a230 |  |
| joy2_y | fn | 2 | 0x40a280 |  |
| join | stmt | 2 | 0x40b280 | yes |
| keyboard | fn | 1 | 0x40b370 | yes |
| last_element | fn | 0 | 0x40cb50 |  |
| last_animation_frame | fn | 1 | 0x4067b0 |  |
| lift | stmt | 3 | 0x40ac70 |  |
| light_color_set | stmt | 4 | 0x406960 | yes |
| light_color_red | stmt | 1 | 0x406a00 |  |
| light_color_green | stmt | 1 | 0x406a70 |  |
| light_color_blue | stmt | 1 | 0x406ae0 |  |
| light_intensity_set | stmt | 2 | 0x406b50 | yes |
| light_intensity | stmt | 1 | 0x406bd0 |  |
| limit | fn | 3 | 0x40c630 |  |
| local | fn | 2 | 0x406650 |  |
| local_set | fn | 3 | 0x406680 |  |
| multiply | fn | 2 | 0x407750 |  |
| off | stmt | 1 | 0x40bff0 | yes |
| on | stmt | 1 | 0x40c020 | yes |
| on_list | fn | 3 | 0x40c0d0 |  |
| on_range | fn | 2 | 0x40c150 |  |
| over_ground | stmt | 2 | 0x409440 |  |
| over_ground_g | stmt | 3 | 0x4093d0 |  |
| pause | stmt | 0 | 0x40ff20 | yes |
| play_set | stmt | 5 | 0x404ba0 |  |
| play_sound | stmt | 1 | 0x40c200 | yes |
| pocket_add | stmt | 3 | 0x40c9c0 |  |
| pocket_set | stmt | 3 | 0x40c9e0 |  |
| pocket | fn | 2 | 0x40ca00 |  |
| position_copy | stmt | 2 | 0x40c510 | yes |
| position_x | fn | 1 | 0x40c540 | yes |
| position_y | fn | 1 | 0x40c560 | yes |
| position_z | fn | 1 | 0x40c580 | yes |
| position_x_add | stmt | 2 | 0x40c770 |  |
| position_y_add | stmt | 2 | 0x40c790 | yes |
| position_z_add | stmt | 2 | 0x40c7b0 |  |
| position_x_set | stmt | 2 | 0x40c7d0 | yes |
| position_y_set | stmt | 2 | 0x40c7f0 | yes |
| position_z_set | stmt | 2 | 0x40c810 | yes |
| random_position_xz | stmt | 5 | 0x40c6a0 |  |
| rebound_x | stmt | 1 | 0x409a90 |  |
| rebound_y | stmt | 1 | 0x409ab0 |  |
| rebound_z | stmt | 1 | 0x409ad0 |  |
| resume | stmt | 0 | 0x4065f0 |  |
| resume_x | stmt | 1 | 0x406630 |  |
| rotation_copy | stmt | 2 | 0x40c8f0 |  |
| rotation_x | fn | 1 | 0x40c710 | yes |
| rotation_y | fn | 1 | 0x40c730 | yes |
| rotation_z | fn | 1 | 0x40c750 | yes |
| rotation_x_add | stmt | 2 | 0x40c830 | yes |
| rotation_y_add | stmt | 2 | 0x40c850 | yes |
| rotation_z_add | stmt | 2 | 0x40c870 | yes |
| rotation_x_set | stmt | 2 | 0x40c890 | yes |
| rotation_y_set | stmt | 2 | 0x40c8b0 | yes |
| rotation_z_set | stmt | 2 | 0x40c8d0 | yes |
| samplerate_set | stmt | 2 | 0x40cc90 |  |
| save | stmt | 0 | 0x4065d0 |  |
| save_x | stmt | 1 | 0x406610 |  |
| scale_set | stmt | 4 | 0x406810 | yes |
| scale_x | stmt | 1 | 0x406850 |  |
| scale_y | stmt | 1 | 0x406870 |  |
| scale_z | stmt | 1 | 0x406890 |  |
| score_add | stmt | 2 | 0x40c960 | yes |
| score_set | stmt | 2 | 0x40c980 | yes |
| score | fn | 1 | 0x40c9a0 | yes |
| show | fn | 1 | 0x40c090 | yes |
| signal_reset | stmt | 2 | 0x40ca20 |  |
| signal_set | stmt | 2 | 0x40ca40 |  |
| signal | fn | 2 | 0x40ca60 |  |
| sight | fn | 4 | 0x40a610 |  |
| sound_mode_set | stmt | 2 | 0x40c1e0 |  |
| sound_rate_set | stmt | 2 | 0x40cc90 | yes |
| sound_status | fn | 1 | 0x40c170 |  |
| speed | fn | 1 | 0x40cdb0 |  |
| speed_lz | fn | 1 | 0x40cdd0 |  |
| speed_set | stmt | 2 | 0x40d090 | yes |
| speed_xz | fn | 1 | 0x40cd90 |  |
| speed_xz_set | stmt | 2 | 0x40d040 | yes |
| spin_copy | stmt | 2 | 0x40c4b0 |  |
| spin_update | stmt | 1 | 0x40a900 | yes |
| spin_x | fn | 1 | 0x40cc30 |  |
| spin_y | fn | 1 | 0x40cc50 |  |
| spin_z | fn | 1 | 0x40cc70 |  |
| spin_x_add | stmt | 2 | 0x40cb70 |  |
| spin_y_add | stmt | 2 | 0x40cb90 |  |
| spin_z_add | stmt | 2 | 0x40cbb0 |  |
| spin_x_set | stmt | 2 | 0x40cbd0 |  |
| spin_y_set | stmt | 2 | 0x40cbf0 | yes |
| spin_z_set | stmt | 2 | 0x40cc10 |  |
| sprite_3d_pos | stmt | 5 | 0x4186e0 |  |
| sprite_clip_set | stmt | 5 | 0x4066e0 |  |
| sprite_clip_x1 | stmt | 1 | 0x406710 |  |
| sprite_clip_x2 | stmt | 1 | 0x406730 |  |
| sprite_clip_y1 | stmt | 1 | 0x406750 |  |
| sprite_clip_y2 | stmt | 1 | 0x406770 |  |
| sprite_picked | stmt | 1 | 0x4068b0 |  |
| state_reset | stmt | 2 | 0x40ca80 |  |
| state_set | stmt | 2 | 0x40caa0 |  |
| state | fn | 2 | 0x40cac0 | yes |
| status | fn | 1 | 0x40a930 |  |
| steer | stmt | 3 | 0x40a960 |  |
| steer_angle_to | fn | 3 | 0x40bad0 |  |
| stop_sound | stmt | 1 | 0x40c440 | yes |
| stop_sound_system | stmt | 0 | 0x404e50 |  |
| restart_sound_system | stmt | 0 | 0x405030 |  |
| stop_task | stmt | 0 | 0x4050a0 |  |
| subtract | fn | 2 | 0x4076e0 |  |
| sum | fn | 2 | 0x407700 |  |
| thrust | stmt | 2 | 0x40b3c0 |  |
| thrust_xz | stmt | 2 | 0x40b570 | yes |
| timer | fn | 1 | 0x40cd50 |  |
| timer_set | stmt | 2 | 0x40cd70 |  |
| track | stmt | 2 | 0x40bb50 | yes |
| track_play | stmt | 2 | 0x40d3c0 |  |
| track_play_loop | stmt | 2 | 0x40d410 |  |
| track_play_back | stmt | 2 | 0x40d480 |  |
| track_play_reset | stmt | 2 | 0x40d4f0 |  |
| track_yr | stmt | 2 | 0x40bc50 | yes |
| translucent | stmt | 2 | 0x451ed0 | yes |
| type | stmt | 1 | 0x406fb0 |  |
| velocity | fn | 1 | 0x40cf80 |  |
| velocity_xz | fn | 1 | 0x40cff0 |  |
| velocity_copy | stmt | 2 | 0x40c4e0 |  |
| velocity_set | stmt | 2 | 0x40d090 |  |
| velocity_set_xz | stmt | 2 | 0x40d040 |  |
| velocity_x | fn | 1 | 0x40c5a0 |  |
| velocity_y | fn | 1 | 0x40c5c0 |  |
| velocity_z | fn | 1 | 0x40c5e0 |  |
| velocity_x_add | stmt | 2 | 0x40d240 |  |
| velocity_y_add | stmt | 2 | 0x40d260 | yes |
| velocity_z_add | stmt | 2 | 0x40d280 |  |
| velocity_x_set | stmt | 2 | 0x40d2a0 | yes |
| velocity_y_set | stmt | 2 | 0x40d2c0 | yes |
| velocity_z_set | stmt | 2 | 0x40d2e0 | yes |
| velocity_update | stmt | 1 | 0x40a5e0 | yes |
| visual_color_set | stmt | 4 | 0x406c50 |  |
| visual_rgba_set | stmt | 5 | 0x406df0 |  |
| visual_picked | stmt | 1 | 0x417fa0 |  |
| visual_picked_get | fn | 0 | 0x417fd0 |  |
| visual_ray | fn | 2 | 0x4182d0 |  |
| wrap | fn | 3 | 0x40c650 |  |
| atan_512 | fn | 1 | 0x4078a0 |  |
| attract_point | stmt | 5 | 0x4098a0 |  |
| attract_xr | stmt | 3 | 0x409950 |  |
| attract_y | stmt | 3 | 0x4097c0 |  |
| attract_yr | stmt | 3 | 0x4099f0 |  |
| attract_zr | stmt | 3 | 0x409a40 |  |
| bottom | fn | 1 | 0x407400 |  |
| boundary_rebound | stmt | 2 | 0x408970 |  |
| ground_set | stmt | 1 | 0x4189b0 | yes |
| clip_distance | stmt | 0 | 0x42fad0 |  |
| cos_512 | fn | 1 | 0x407800 | yes |
| element_id_set | stmt | 2 | 0x40caf0 |  |
| element_id | fn | 1 | 0x40cb10 | yes |
| execute | fn | 1 | 0x407110 | yes |
| fog_set | stmt | 2 | 0x42f9f0 |  |
| fog_start | stmt | 0 | 0x42fa90 |  |
| fog_end | stmt | 0 | 0x42fab0 |  |
| fog_color_set | stmt | 3 | 0x42faf0 |  |
| friction_xr | stmt | 2 | 0x40ba00 |  |
| friction_yr | stmt | 2 | 0x40ba30 |  |
| friction_zr | stmt | 2 | 0x40ba60 |  |
| ground_color | fn | 1 | 0x4078e0 |  |
| ground_color_rebound | stmt | 2 | 0x408970 |  |
| hearer | stmt | 1 | 0x4070f0 |  |
| join_pos | stmt | 2 | 0x40b2f0 | yes |
| listener | stmt | 1 | 0x4070f0 | yes |
| mouse_on | stmt | 0 | 0x407050 |  |
| mouse_off | stmt | 0 | 0x407070 |  |
| mouse_set | stmt | 2 | 0x45f620 |  |
| mouse_x | fn | 0 | 0x407090 |  |
| mouse_y | fn | 0 | 0x4070b0 |  |
| mouse_click | fn | 0 | 0x4070d0 |  |
| nearest_element | fn | 2 | 0x407330 | yes |
| orient_straight | stmt | 3 | 0x40bce0 |  |
| orient_forward | stmt | 5 | 0x40bd30 |  |
| over_ground_slope | stmt | 3 | 0x407a40 |  |
| random | fn | 2 | 0x40c670 | yes |
| random_element | fn | 1 | 0x407270 |  |
| sin_512 | fn | 1 | 0x4077b0 | yes |
| radius | fn | 1 | 0x407130 |  |
| radius_max | fn | 1 | 0x407150 |  |
| register | fn | 2 | 0x407170 |  |
| register_set | fn | 3 | 0x407190 |  |
| sprite_dim_x | fn | 1 | 0x407210 |  |
| sprite_dim_y | fn | 1 | 0x407240 |  |
| sqrt_512 | fn | 1 | 0x407770 |  |
| tan_512 | fn | 1 | 0x407850 |  |
| track_record | stmt | 1 | 0x40d320 |  |
| volume_rebound | stmt | 4 | 0x41a070 |  |
| volume_slide | stmt | 5 | 0x418e10 |  |
| volume_set | stmt | 1 | 0x4189d0 |  |
| year | stmt | 0 | 0x45c4f0 |  |
| month | stmt | 0 | 0x45c510 |  |
| day | stmt | 0 | 0x45c530 |  |
| hour | stmt | 0 | 0x45c550 |  |
| minute | stmt | 0 | 0x45c570 |  |
| second | stmt | 0 | 0x45c590 |  |
| output_b | stmt | 2 | 0x4102c0 |  |
| output_w | stmt | 2 | 0x4102f0 |  |
| output_d | stmt | 2 | 0x410320 |  |
| input_b | stmt | 1 | 0x410230 |  |
| input_w | stmt | 1 | 0x410260 |  |
| input_d | stmt | 1 | 0x410290 |  |
| oscillate | stmt | 3 | 0x410200 |  |
| joy1_z | stmt | 2 | 0x40a1e0 |  |
| joy2_z | stmt | 2 | 0x40a2d0 |  |
| joy1_xr | stmt | 0 | 0x40a320 |  |
| joy1_yr | stmt | 0 | 0x40a340 |  |
| joy1_zr | stmt | 0 | 0x40a360 |  |
| joy2_xr | stmt | 0 | 0x40a380 |  |
| joy2_yr | stmt | 0 | 0x40a3a0 |  |
| joy2_zr | stmt | 0 | 0x40a3c0 |  |
| joy1_dir1 | stmt | 0 | 0x40a3e0 |  |
| joy1_dir2 | stmt | 0 | 0x40a420 |  |
| joy1_dir3 | stmt | 0 | 0x40a460 |  |
| joy1_dir4 | stmt | 0 | 0x40a4a0 |  |
| joy2_dir1 | stmt | 0 | 0x40a4e0 |  |
| joy2_dir2 | stmt | 0 | 0x40a520 |  |
| joy2_dir3 | stmt | 0 | 0x40a560 |  |
| joy2_dir4 | stmt | 0 | 0x40a5a0 |  |
| globals_io_set | stmt | 2 | 0x41aea0 |  |
| play_task_status | stmt | 0 | 0x4050d0 |  |
| spline_key_add | stmt | 3 | 0x437f60 |  |
| spline_key_set | stmt | 5 | 0x438010 |  |
| spline_key_remove | stmt | 2 | 0x4380b0 |  |
| spline | stmt | 4 | 0x438100 |  |
| spline_pos_x | stmt | 3 | 0x438220 |  |
| spline_pos_y | stmt | 3 | 0x438310 |  |
| spline_pos_z | stmt | 3 | 0x438400 |  |
| globals_sort | stmt | 2 | 0x4384f0 |  |
| track_record_reset | stmt | 0 | 0x40d300 |  |
| push | stmt | 1 | 0x417b10 |  |
| pop | stmt | 1 | 0x417b50 |  |
| box_collisions_off | stmt | 1 | 0x436c10 |  |
| box_collisions_on | stmt | 1 | 0x436bf0 |  |
| rebound | stmt | 2 | 0x4189f0 |  |
| rebound_yr | stmt | 2 | 0x418c60 |  |
| rebound_yr_factor | stmt | 1 | 0x418df0 |  |
| collision_check | stmt | 2 | 0x44ccd0 |  |
| race_lap | stmt | 1 | 0x42a270 |  |
| race_lap_set | stmt | 2 | 0x42a2c0 |  |
| race_next_checkpoint | stmt | 1 | 0x42a300 |  |
| race_next_checkpoint_set | stmt | 2 | 0x42a340 |  |
| race_on | stmt | 1 | 0x42a180 |  |
| race_off | stmt | 1 | 0x42a1f0 |  |
| race_position | stmt | 1 | 0x42a380 |  |
| checkpoint | stmt | 2 | 0x42a4b0 |  |
| checkpoint_add | stmt | 4 | 0x42a760 |  |
| checkpoint_reset | stmt | 0 | 0x42a7b0 |  |
| checkpoint_height_set | stmt | 2 | 0x42a800 |  |
| track_smooth | stmt | 3 | 0x40bb80 |  |
| track_yr_smooth | stmt | 3 | 0x40bc70 |  |
| speed_xz_limit | stmt | 2 | 0x429fc0 |  |
| machine_number_set | stmt | 1 | 0x4460c0 |  |
| machine_number | stmt | 0 | 0x4460f0 |  |
| machine_status | stmt | 1 | 0x446110 |  |
| network_mismatch | stmt | 0 | 0x446090 |  |
| network_mode | stmt | 0 | 0x446160 |  |
| element_find | stmt | 4 | 0x445e70 |  |
| machine_element_find | stmt | 4 | 0x445f50 |  |
| machine_element | stmt | 1 | 0x446070 |  |
| disable | stmt | 1 | 0x4461a0 |  |
| enable | stmt | 1 | 0x4461c0 |  |
| host | stmt | 0 | 0x446180 |  |

## 6. Unknown / open
- Compiler details (operator precedence, truthiness of bare values, `goto`/labels/`run`/`play`), exact random seeding, pitch/yaw sign conventions.
- Display-side effects of `translucent`, `animate`, sprite viewports (fields 36-38), and the Score Counter drawing (type 8).
- multilev.ini row meaning and mapping from `exit(n)` to the next project; globals_save/globals_load; high-score persistence.
- `.col` contents (maybe unused at runtime); `dbg_color_set` absent from table.
