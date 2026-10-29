#!/bin/bash
# Fetches the pinned SDL2 source into android/app/jni/SDL and SDL's Java glue into the app.
# (No gl4es: the renderer targets GLES2, which Android provides natively.)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$ROOT/deps.lock"
DEST="$ROOT/android/app/jni/SDL"
if [ -d "$DEST/.git" ] && [ "$(git -C "$DEST" rev-parse HEAD)" = "$SDL_COMMIT" ]; then
    echo "ok   SDL @ ${SDL_COMMIT:0:10}"
else
    rm -rf "$DEST"; git init -q "$DEST"
    git -C "$DEST" remote add origin "$SDL_REPO"
    git -C "$DEST" fetch -q --depth 1 origin "$SDL_REF"
    git -C "$DEST" checkout -q FETCH_HEAD
    got="$(git -C "$DEST" rev-parse HEAD)"
    [ "$got" = "$SDL_COMMIT" ] || { echo "ERROR: SDL $SDL_REF is $got, expected $SDL_COMMIT" >&2; exit 1; }
    echo "got  SDL @ ${SDL_COMMIT:0:10}"
fi
JAVA="$ROOT/android/app/src/main/java/org/libsdl/app"
rm -rf "$JAVA"; mkdir -p "$JAVA"
cp "$DEST/android-project/app/src/main/java/org/libsdl/app/"*.java "$JAVA/"
echo "ok   SDL Java glue"
