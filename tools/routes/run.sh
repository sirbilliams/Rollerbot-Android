#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
exec timeout 900 "${RB_ROUTES_EXE:-$root/build/src/brain/tests/rbbrain}" \
  "${RB_GAME_DIR:-$root/.game}" --route-chain "$root/tools/routes"
