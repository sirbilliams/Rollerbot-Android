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
# Android 14 (targetSdk 34) throws from registerReceiver without RECEIVER_(NOT_)EXPORTED, which kills SDL's HID
# gamepad support ("hid_init threw an exception"): pass RECEIVER_EXPORTED (2) on API 33+ as newer SDL does.
sed -i -E 's/mContext\.registerReceiver\((m[A-Za-z]+), filter\);/if (android.os.Build.VERSION.SDK_INT >= 33) mContext.registerReceiver(\1, filter, 2); else mContext.registerReceiver(\1, filter);/' "$JAVA/HIDDeviceManager.java"
grep -q "SDK_INT >= 33" "$JAVA/HIDDeviceManager.java" || { echo "ERROR: HID registerReceiver patch did not apply" >&2; exit 1; }
echo "ok   SDL Java glue (HID receiver patched)"
