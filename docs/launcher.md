# Launcher, companion, saves and pause menu

All original code. Nothing here uses game art, game fonts or a trademarked logo: the wordmark is plain text set in toy colours.

## Android launcher (`SetupActivity` + `LauncherView` + `Toy`)
Drawn entirely on a Canvas (no bitmaps) on a 1920 x 1080 stage, letterboxed; the world behind it fills the whole view.
Each page is an era of the time journey, and a red ball rolls through it:
Play = desert and pyramids, Import = castle, Settings = city at dusk, About = space. Changing page wipes the next era in
from the left behind a bright edge, the ball hops, and the card slides in. Floating toy cubes and balls drift in front.
- **Play**: stat chips (level, high score, last score), one dot per level (lit = unlocked, ring = the one that would start),
  rows Continue (the saved level) / New game (from the title) / Play level (Left/Right over unlocked levels). No game yet:
  "Import your game first".
- **Import**: disc folder, image or .zip, language, remove (asks twice), progress bar. Same importer as before.
- **Settings**: Widescreen (Fill, Side panels, Stretch, Original 4:3), Controls (A and X jump / A jumps / X jumps,
  exported as RB_PADJUMP), Bottom screen on/off. Changes apply when the game starts.
- **About**: version, legal line.
Input: D-pad / left stick (latched at 0.6, released under 0.3) move, A / Enter select, B / Back step back then quit;
Left/Right adjust a row. Touch: tap a page button or row (tap the right part of an adjustable row to step up, the middle
to select); a tap in the world makes the ball hop. The launcher reads the game's save folder (`SaveInfo`: `rollerbot.cfg`
and global 100 of `globvars.dat`); `RollerActivity` passes `RB_SAVE` (app files/save), `RB_LEVEL` (from Continue / Play
level), `RB_PADJUMP`, `RB_WIDESCREEN`, `RB_DATA`.

## Bottom screen (`CompanionPanel` Presentation + `CompanionView`)
Same style, 1240 x 1080 stage: **Now** (level or "Title screen", lives, fruit, points, high score, lives as balls, Pause /
Resume, Restart level; the heading says Paused or Game menu open), **Levels** (a ball per level, locked ones grey, tap to jump),
**Screen** (widescreen mode, controls). It polls `rb::Bridge` every 250 ms (`Native.status`, now with high score, reached
level and menu-open flag) and sends pause, restart, level and widescreen requests back.

## Saves (`src/game/Save.*`, `rb::GameSave` over `rb::SaveStore`)
- Folder: `RB_SAVE`, else the app files on Android, else `$XDG_DATA_HOME/rollerbot` (Flatpak sets it to its data
  folder) or `~/.local/share/rollerbot`.
- `globvars.dat` (2048 LE32, the exe's global bank) holds the **high score at global 100** (byte 400), never lowered. The game
  puts the saved value into global 100 on every level load and writes it back when a brain raises it (at most every 5 s,
  on level change, menu, suspend and exit). Other globals in the file are preserved, not restored (lives and score start fresh).
- `rollerbot.cfg` (key=value): `reached` (highest level number unlocked, set when a level loads), `last` (level to
  continue), `score`, `widescreen` (used when `RB_WIDESCREEN` is not set), `bottom`.
- `RB_CONTINUE=1` starts at the last level instead of the title; `RB_LEVEL` still wins.

## Desktop pause menu (`src/game/Game.cpp`, `MenuRaster.*`)
Esc (not on the title screen, where Esc still leaves as in the exe), P, Tab, or pad Start / Back opens it. Items: Resume,
Restart level, Level (Left/Right over unlocked levels, Enter loads), Screen (widescreen mode, saved), Quit game. The
game and audio freeze while it is open. The panel is rasterised on the CPU with the game's own font (`font.fnt`) into a
640 x 480 canvas and drawn through the HUD overlay. `RB_MENU=<frame>` opens it for screenshots.

## Renders
`tools/launcher/render.sh [dir]` (inside the omtbuild box; default `.shots/launch/`) compiles the real `Toy`, `LauncherView`,
`CompanionView` and `Native` against Java2D shims (`tools/launcher/shim`), runs the navigation / stick-latch / touch checks,
and renders `launcher-{play,import,settings,about}.png` at 1920x1080 and `bottom-{now,levels,screen,paused}.png` at 1240x1080.
Android compile check: `javac -cp android.jar:<SDLActivity stubs> android/app/src/main/java/io/github/sirbilliams/rollerbot/*.java`
(only the SDL stub's missing `nativePause/nativeResume` complain).
