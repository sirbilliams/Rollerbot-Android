#!/usr/bin/env bash
# Runs inside the emulator (reactivecircus/android-emulator-runner). Output goes to ./smoke/.
# Fails on: FATAL EXCEPTION, native crash (Fatal signal / tombstone / DEBUG), ANR, or our process dying.
set -u
PKG=io.github.sirbilliams.rollerbot
OUT=smoke; mkdir -p "$OUT"
FAIL=0
fail() { echo "::error::$*"; FAIL=1; }
shot() { adb exec-out screencap -p > "$OUT/$1.png" || echo "screencap $1 failed"; }
alive() { adb shell pidof "$1" > /dev/null 2>&1; }   # $1 = process name
key() { adb shell input keyevent "$1"; sleep 2; }

adb shell settings put secure immersive_mode_confirmations confirmed   # no 'Viewing full screen' hint in shots
adb logcat -c
adb logcat -v threadtime > "$OUT/logcat-live.txt" 2>&1 &
LOGPID=$!

adb install -r app-debug.apk || { echo "::error::install failed"; exit 1; }
adb shell pm grant "$PKG" android.permission.POST_NOTIFICATIONS 2>/dev/null || true

# 1. Launcher + its pages (pad navigation)
adb shell am start -W -n "$PKG/.SetupActivity" || fail "launcher failed to start"
sleep 8; shot launcher-00
alive "$PKG" || fail "app process not running after launcher start"
# The launcher's own controls: Up/Down picks a page in the left column, Right opens its panel, B/Back leaves it
# (Left adjusts a setting row, so it does not always return).
for i in 1 2 3; do
  key KEYCODE_DPAD_DOWN;  shot "launcher-page-$i"
  key KEYCODE_DPAD_RIGHT; shot "launcher-page-$i-open"
  key KEYCODE_BACK
done
key KEYCODE_DPAD_CENTER; shot launcher-enter
key KEYCODE_BACK
alive "$PKG" || fail "app process died during launcher navigation"

# The pad must have moved the launcher: LauncherView logs "launcher page N" on each page change
sleep 1; adb logcat -d -s Rollerbot:I > "$OUT/launcher-nav.txt"
PAGES=$(grep -c "launcher page" "$OUT/launcher-nav.txt" || true)
[ "${PAGES:-0}" -ge 3 ] || fail "DPAD did not change launcher pages ($PAGES page changes logged, want >= 3)"
# and the screenshots differ (pages have different eras/art)
[ "$(md5sum "$OUT"/launcher-page-*-down.png | awk '{print $1}' | sort -u | wc -l)" -ge 3 ] || fail "launcher screenshots look identical"

# 2. Game activity with no game data: must show the stub/import prompt, not crash
adb shell am start -W -n "$PKG/.RollerActivity" || fail "game activity failed to start (not exported? built without -PemuAbi?)"
sleep 12; shot game-nodata
alive "$PKG:game" || echo "note: :game process not running (it may have returned to the launcher)"
alive "$PKG" || fail "app process died after game start"

# 3. Second display (companion Presentation)
adb shell settings put global overlay_display_devices "1240x1080/240"
sleep 6
adb shell dumpsys display | grep -i "Display .*overlay\|mBaseDisplayInfo.*Overlay" | head -5 > "$OUT/displays.txt" || true
adb shell am start -W -n "$PKG/.SetupActivity" > /dev/null
sleep 4
adb shell am start -W -n "$PKG/.RollerActivity" > /dev/null
sleep 10
for d in $(adb shell dumpsys SurfaceFlinger --display-id 2>/dev/null | grep -o "Display [0-9]*" | awk '{print $2}'); do
  adb exec-out screencap -p -d "$d" > "$OUT/display-$d.png" 2>/dev/null || true
done
shot display-default
adb shell dumpsys activity activities | grep -i "display\|rollerbot" | head -30 > "$OUT/activities.txt" || true
alive "$PKG" || fail "app process died with second display"

# 4. Verdict from logcat
sleep 2; kill "$LOGPID" 2>/dev/null
adb logcat -d -v threadtime > "$OUT/logcat.txt"
rm -f "$OUT/logcat-live.txt"
if grep -E "FATAL EXCEPTION|Fatal signal|SIGSEGV|SIGABRT|SIGILL|SIGBUS|>>> $PKG|tombstoned.*$PKG|ANR in $PKG|Process $PKG.* has died|am_crash" "$OUT/logcat.txt" > "$OUT/crash-lines.txt"; then
  echo "::error::crash markers in logcat:"; head -30 "$OUT/crash-lines.txt"; FAIL=1
fi
adb shell ls /data/tombstones 2>/dev/null | grep -q . && echo "note: tombstones present (see logcat)" || true
echo "smoke result: $([ $FAIL = 0 ] && echo PASS || echo FAIL)"
exit $FAIL
