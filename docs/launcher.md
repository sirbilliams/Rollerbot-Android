# Launcher, companion, saves and pause menu

All original code. Nothing here uses game art, game fonts or a trademarked logo: the wordmark is plain text set in toy colours.

## Menu structure (every option lives in exactly one place; names are the same everywhere)
- **Launcher, Play**: status tiles (level / high score / last score) + Continue, New game, Play level. One notice row (with a link
  to Game files) when the chosen language is not installed.
- **Launcher, Game files**: status block (found or not, language chips EN FR DE ES IT, size, import progress) + Import from disc
  folder, Import from image or .zip, Remove game files (asks twice). No language selector here.
- **Launcher, Settings** (section headers in the card, the pad skips them; scrolling keeps the current section header in view):
  DISPLAY Widescreen, Bottom screen, Menu layout (Tabs / Stack) · CONTROLS Jump button (A and X / A only / X only), Controls
  reference (opens the move/jump/menu text) · SOUND Volume · LANGUAGE Game language (installed sets only; also the language
  the next import opens first).
- **Launcher, About**: what it is, credits, legal note, version.
- **In-game pause menu** (desktop Esc / Start; `MenuRaster`): Resume, Restart level, Level select, then a Settings header with
  Widescreen, Volume, Game language (only with 2+ installed sets, switches the asset set at once and restarts the level),
  then Quit game. The game learns the sets from `RB_LANGS="english=/dir;french=/dir"` (Android and Flatpak set it) and writes
  the running set to `rollerbot.cfg` (`lang=`); the launcher adopts it when it comes back to the front.
- **Bottom screen in game** (`CompanionView`): Now (status, Pause, Restart level), Levels, Settings (Display: Widescreen; Sound:
  Volume; Language: Game language).
- **Bottom screen in the launcher** (`LauncherPanel` + `LauncherBottomView`): see below.

## Android launcher (`SetupActivity` + `LauncherView` + `Toy`)
Drawn entirely on a Canvas (no bitmaps) on a 1920 x 1080 stage, letterboxed; the world behind it fills the whole view. Each page
is an era of the time journey with a red ball rolling through it (Play desert, Game files castle, Settings city, About space);
changing page wipes the next era in. Two centred layouts, picked in Settings > Menu layout: **Tabs** (page tabs across the top,
one card below; Left/Right move the tabs, Down or A enters the card) and **Stack** (a stack of four big buttons; choosing one
slides it away and the card takes its place with a Back pill). Input: D-pad / left stick, A / Enter select, B / Back step back
then quit, Left/Right adjust a row; touch taps pages and rows. The launcher reads the game's save folder (`SaveInfo`) and
passes `RB_SAVE`, `RB_LEVEL`, `RB_PADJUMP`, `RB_WIDESCREEN`, `RB_VOLUME`, `RB_LANGS`, `RB_LANG`, `RB_DATA` to the game.

## Launcher on the bottom screen
While the launcher is in front and Bottom screen is On, a Presentation on the Thor's built-in bottom screen (same display picking
as the in-game companion; nothing happens without one) shows `LauncherBottomView` (1240 x 1080, touch only, same style, follows
the top page's era): four big section buttons that switch the top screen too (and back: it reads the top view's model every
frame), then the section as large touch controls: Play = level grid (locked tiles dimmed) + big Continue + the save tiles; Game
files = status, language chips, import buttons, progress; Settings = the same rows as two-column toggles under their headers;
About = the text. Pad focus stays on the top screen. When the game starts the launcher pauses, the Presentation is dismissed and
the in-game companion takes over.

## Saves (`src/game/Save.*`, `rb::GameSave` over `rb::SaveStore`)
- Folder: `RB_SAVE`, else the app files on Android, else `$XDG_DATA_HOME/rollerbot` (Flatpak sets it to its data
  folder) or `~/.local/share/rollerbot`.
- `globvars.dat` (2048 LE32, the exe's global bank) holds the **high score at global 100** (byte 400), never lowered. The game
  puts the saved value into global 100 on every level load and writes it back when a brain raises it (at most every 5 s,
  on level change, menu, suspend and exit). Other globals in the file are preserved, not restored (lives and score start fresh).
- `rollerbot.cfg` (key=value): `reached` (highest level number unlocked, set when a level loads), `last` (level to
  continue), `score`, `widescreen` (used when `RB_WIDESCREEN` is not set), `bottom`, `volume` (0-100; `RB_VOLUME` wins).
- `RB_CONTINUE=1` starts at the last level instead of the title; `RB_LEVEL` still wins.

## Renders
`tools/launcher/render.sh [dir]` (inside the omtbuild box; default `.shots/launch/`) compiles the real `Toy`, `LauncherView`,
`CompanionView` and `Native` against Java2D shims (`tools/launcher/shim`), runs the navigation / stick-latch / touch checks,
and renders `A_*.png` (Tabs) and `B_*.png` / `B_*_menu.png` (Stack) per page at 1920x1080, `bottom-A_*.png` / `bottom-B_*.png` (the launcher's bottom screen) and `bottom-game-{now,levels,screen,paused}.png` (in-game companion) at 1240x1080.
Android compile check: `javac -cp android.jar:<SDLActivity stubs> android/app/src/main/java/io/github/sirbilliams/rollerbot/*.java`
(only the SDL stub's missing `nativePause/nativeResume` complain).
