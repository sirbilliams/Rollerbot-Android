# Game lane: rendering behaviour recovered from Rollerbot.exe

Addresses are in Rollerbot.exe; all of it is implemented in `src/game/Game.cpp` unless marked GUESS.

## Model descriptors (`1|tex|mesh|...` lines; loader inside 0x40221b; 1-based field numbers)
- One line per mesh layer. `[4]` texture mapping: 0 none (face colour `[25..27]/256`), 1/2 flat box projection, 3 cylinder, 4 sphere, 5 chrome
  (CreateWrap types 0/1/2/3). `[29]` picks one of six axes. Flat wrap (dir, up): 1 (0,0,-1)(0,-1,0), 2 (1,0,0)(0,-1,0), 3 (0,0,1)(0,-1,0),
  4 (-1,0,0)(0,-1,0), 5 (0,1,0)(0,0,-1), 6 (0,-1,0)(0,0,-1). Cylinder dir (0,1,0) for 1..4 (up (0,0,1),(-1,0,0),(0,0,-1),(1,0,0)), dir (0,0,1) for 5/6.
- `[12]` texture scale k = v*1000/2^28 per metre (flat su=sv=k; cylinder su=1, sv=-k). Offsets: flat ou=-([2a+11]+127.5)/256, ov=-([2a+12]+127.5)/256;
  cylinder ov=-([2a+13]+127.5)/256. `[10]` != 0: camera layer, faces 50% transparent (0x80ffffff). `[11]` != 0: decal transparency (colour key black).
- 3DS files carry no mapping chunk; the wrap is the only UV source. Mesh coordinates: 3DS Z-up mapped to game Y-up (x,z,y), winding flipped.
- Cylinder/sphere wraps turn around `dir` (game Y): verified on alien/player faces. The black rectangle on aliens was the wrong axis, not a missing key.

## Per-level settings: last 512 bytes of the .col
0xfe00/0xfe04 fog start/end (m); 0xfe08 ARGB background+fog colour (also `3dbg_color_set`); 0xfe0c sky scale = far clip; 0xfe10/0xfe14 sky texture rows
(default high 479); 0xfe18 hood index (`hood<N>a.x`/`b.x`); 0xfe1c option flags (0x10 = fog on); 0xfe20 percent (front clip = pct/100).
`.prj` line after the objects, `./HASH.dat|N|`: sky bitmap. Fog is linear.

## Sky (0x402216 from 0x436b30)
Two X hemispheres (pole on file Z) scaled by skyScale-(101-pct)*skyScale/1000, cylinder wrap dir (0,1,0) up (0,0,-1), ov=hi/F, su=-1, sv=-1/F,
hi=(rowHigh-238)/240*s, lo=(rowLow-242)/240*s, F=hi-lo; z-buffer off, follows the camera. File Z mapped to world Y, winding flipped (GUESS from the picture).

## Camera (0x4012f8)
SetField(240*pct/100 / -zoom), zoom = int 12 of the d30 record (-128 -> 1.875; editor default 0.9375), SetFront(pct/100), SetBack(skyScale).
Which zoom the camera field `20&30` selects is unresolved; 1.875 follows the code as written.

## Elements, units, HUD
- Types: 0 camera, 2 world billboard, 5 sprite overlay, 8 counter, 10 light, 11 model, 12 collision-only ground. Types 5/8 are screen pixels (brains write raw pixels).
- Counters draw the credits register with a digit strip: cells 0-9 then + - ., cell 17 px (211 px strip) or 15 px (166 px strip), left aligned (GUESS for alignment/zeros).
- Field 34 of a .prj record is the editor's variable snapshot (`count/type/value/slot...`): userInt/userMet values override source defaults (see Brain.cpp).
- HUD has its own 640x480 space: Fill = 4:3 fitted inside the whole window (3D is the cropped picture); Side panels/Off = the 4:3 game area; Stretch = whole window.
- Empty-star slots (20D86FF8) are genuinely dark boxes with a white frame; black corners are the colour key.

## Lights (checked)
0x40178f adds a white (1,1,1) ambient light to the root at start-up; 0x4011c7 adds one light per light element (colour from the element, linear attenuation
1-(v&0x3ff)/1024). D3DRM sums lights, so the white ambient already saturates every surface: element lights and `light_color_set` (0x406960) cannot change the
picture. We render fully lit and ignore them on purpose.

## Loop
Brains tick at the rate in Brain.h; display runs at the platform rate with positions/rotations/scale interpolated between the last two ticks (teleports >30 m and
on/off edges are not blended). Input is sampled when a brain reads it. `RB_RUNSECS=N` quits after N s and logs frame times and key-to-tick latency.
