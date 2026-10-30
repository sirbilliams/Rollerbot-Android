# Rollerbot: A Time Journey reconstruction for Android / Linux

## Layout
`src/platform`: window, loop, input, layout (`rb::Platform`) · `src/game`: asset store, brain VM, scene
`android/`: Gradle app (launcher, game activity) · `packaging/flatpak/`: Flatpak manifest

## Build
`cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build`
`bash fetch-deps.sh; cd android && ./gradlew assembleDebug`

## Test
`ctest --test-dir build` · CI `workflow/build.yml`: desktop ctest + Android APK/Smoke (CI `emulator.yml`)
`RB_DATA=/path/to/game ./build/src/platform/rollerbot` (interactive test)

## Rules
Never commit game files, ROMs, or ISOs. Keep signing secrets out of git (use CI secrets). Patch SDL2 Java glue for API 34+ (`RECEIVER_EXPORTED`). Verify JNI changes with `rb_jni_check`.

## Release
APK published to GitHub Releases via CI push to `main`. Flatpak built via `packaging/flatpak/io.github.sirbilliams.Rollerbot.yml`.
