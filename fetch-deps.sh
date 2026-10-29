#!/bin/bash
# Fetches the pinned SDL2 + unshield sources into android/app/jni/SDL and SDL's Java glue into the app.
# (No gl4es: the renderer targets GLES2, which Android provides natively.)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$ROOT/deps.lock"
fetch() {   # repo ref expected-commit dest
    local repo=$1 ref=$2 want=$3 dest=$4
    if [ -d "$dest/.git" ] && [ "$(git -C "$dest" rev-parse HEAD)" = "$want" ]; then
        echo "ok   $(basename "$dest") @ ${want:0:10}"; return
    fi
    rm -rf "$dest"; git init -q "$dest"
    git -C "$dest" remote add origin "$repo"
    git -C "$dest" fetch -q --depth 1 origin "$ref"
    git -C "$dest" checkout -q FETCH_HEAD
    local got; got="$(git -C "$dest" rev-parse HEAD)"
    [ "$got" = "$want" ] || { echo "ERROR: $repo $ref is $got, expected $want" >&2; exit 1; }
    echo "got  $(basename "$dest") @ ${want:0:10}"
}
DEST="$ROOT/android/app/jni/SDL"
fetch "$SDL_REPO" "$SDL_REF" "$SDL_COMMIT" "$DEST"
# unshield (MIT): reads the game's InstallShield cabinets. Used by the disc importer (src/platform/import).
fetch "$UNSHIELD_REPO" "$UNSHIELD_REF" "$UNSHIELD_COMMIT" "$ROOT/android/app/jni/unshield"
JAVA="$ROOT/android/app/src/main/java/org/libsdl/app"
rm -rf "$JAVA"; mkdir -p "$JAVA"
cp "$DEST/android-project/app/src/main/java/org/libsdl/app/"*.java "$JAVA/"
echo "ok   SDL Java glue"
