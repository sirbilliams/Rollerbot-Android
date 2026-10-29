#!/usr/bin/env bash
# Headless audio check (run inside omtbuild after building): level001 (title, 60 s, music loop) and level003 (the recorded
# full route), mixed offline with RB_AUDIO_DUMP, events logged with RB_AUDIO_LOG, then analysed. Output in .shots/audio/.
# Optional $1: rbassets.py export dir (sample lengths).
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p .shots/audio
run() {  # level frames keys tag
  timeout 300 xvfb-run -a -s '-screen 0 1280x720x24' env RB_DATA=.game RB_LEVEL="$1" RB_FRAMES="$2" RB_KEYS="$3" \
    RB_AUDIO_DUMP=".shots/audio/$4.wav" RB_AUDIO_LOG=1 RB_NOHUD=1 SDL_VIDEO_X11_FORCE_EGL=1 SDL_AUDIODRIVER=dummy \
    build/src/platform/rollerbot >".shots/audio/$4.log" 2>&1
}
route=${RB_ROUTE_L3:-}   # optional rbbrain schedule file for level003
keys=""; [ -n "$route" ] && keys=$(python3 tools/audio/keys2rb.py "$route" 2>/dev/null)
run level001 3700 "57@3500-3510" l1
run level003 2400 "$keys" l3
for t in l1 l3; do python3 tools/audio/analyze.py ".shots/audio/$t.wav" ".shots/audio/$t.log" ${1:+--assets "$1"} | tee ".shots/audio/$t.txt"; done
