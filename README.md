# Rollerbot: A Time Journey: Android / Flatpak port

A fan reconstruction of *Rollerbot: A Time Journey* (2000, 3D Rad engine) for the AYN Thor (dual screen,
gamepad) and Linux (Flatpak). Original code only: no game files, art or decompiled code are in this repo.

**Legal:** bring your own disc. You must own the game; the app imports it from your own copy (a zip or a
folder of its files). Not affiliated with or endorsed by the rights holders.

## Status

About 5%: reconnaissance is done, and the shell everything plugs into exists (SDL2 + GLES2 window and
loop, input, widescreen modes, Android launcher with a second-screen stub, Flatpak, CI). The stub game draws a
test pattern. Assets, scene, brain VM, audio and UI lanes are being built in `src/<lane>`.

## Build (desktop)

```sh
cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build
./build/src/platform/rollerbot            # test pattern; Esc quits
```

Needs SDL2 and GLES2 dev packages. Environment: `RB_WIDESCREEN=fill|panels|stretch|off`, `RB_FULLSCREEN=1`,
`RB_SIZE=1280x720`, `RB_FRAMES=N` (exit after N frames), `RB_SHOT=out.png` (screenshot of the last frame),
`RB_DATA=<game folder>`. Headless: `RB_SHOT=s.png xvfb-run -a ./build/src/platform/rollerbot`.

## Build (Android APK)

```sh
bash fetch-deps.sh                 # pinned SDL2 source + its Java glue
cd android && ./gradlew assembleDebug
```

Needs JDK 17, Android SDK 34, NDK 26.3 and CMake 3.22. App id `io.github.sirbilliams.rollerbot`. A push to
`main` builds the APK and publishes it as the `latest` release (`.github/workflows/build.yml`, which also builds
and tests the desktop version).

## Flatpak

`packaging/flatpak/io.github.sirbilliams.Rollerbot.yml`: see the comment at the top. On first run the launcher asks
for your game folder (the one with `Rollerbot.exe`) and copies it to the app's data folder.

## Layout

`src/platform` window, loop, input, layout, screenshots (`rb::Platform`, `rb::Game`) · `src/assets`, `src/scene`,
`src/brain`, `src/audio`, `src/ui` (other lanes, added automatically when they have a `CMakeLists.txt`) ·
`android/` Gradle app (launcher, game activity, bottom-screen companion, lid suspend hook) · `packaging/flatpak`.
