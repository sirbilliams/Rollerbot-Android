#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p .shots/sweep
# One Xvfb display for the full capture pass, inside omtbuild.
marker=$(mktemp /tmp/rb-sweep-capture.XXXXXX)
trap 'python3 -c "import os,sys; os.unlink(sys.argv[1])" "$marker"' EXIT
export RB_SWEEP_CAPTURE_MARKER="$marker"
status=0
: > .shots/sweep/xvfb.log
timeout 900 xvfb-run -a -n 99 -e .shots/sweep/xvfb.log -s '-screen 0 1280x720x24' bash -euo pipefail <<'CAPTURE' || status=$?
for project in .game/level[0-9][0-9][0-9].prj; do
  level=${project##*/}; level=${level%.prj}
  timeout 120 env RB_DATA=.game RB_LEVEL="$level" RB_FRAMES=150 \
    RB_SHOTS="150:$PWD/.shots/sweep/$level.png" SDL_VIDEO_X11_FORCE_EGL=1 \
    SDL_AUDIODRIVER=dummy build/src/platform/rollerbot >".shots/sweep/$level-render.log" 2>&1
  test -s ".shots/sweep/$level.png"
  printf '%s captured\n' "$level"
done
printf 'complete\n' >"$RB_SWEEP_CAPTURE_MARKER"
CAPTURE
# Some managed boxes reject xvfb-run's cleanup of the shared X server. A marker
# written only after every successful game process distinguishes that from failure.
if ! test -s "$marker"; then if test "$status" -eq 0; then status=1; fi; exit "$status"; fi
if test "$status" -ne 0; then printf 'Xvfb wrapper cleanup status %s (captures completed)\n' "$status"; fi
timeout 60 montage -label '%f' .shots/sweep/level[0-9][0-9][0-9].png \
  -font DejaVu-Sans -pointsize 16 -thumbnail 320x200 -tile 4x -geometry +8+8 \
  -background '#20242b' -fill white .shots/sweep/sheet.png
