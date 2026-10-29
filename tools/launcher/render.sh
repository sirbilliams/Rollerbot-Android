#!/bin/sh
# Render the real Android Canvas launcher and bottom-screen companion with a Java2D graphics shim (no Android needed).
# Usage: tools/launcher/render.sh [output-directory]   (default .shots/launch); run inside the omtbuild box.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d /tmp/rb-launcher-render.XXXXXX)
trap 'rm -rf "$build"' EXIT
out=${1:-"$here/../../.shots/launch"}
mkdir -p "$out"
src="$here/../../android/app/src/main/java/io/github/sirbilliams/rollerbot"
javac -nowarn -d "$build" $(find "$here/shim" -name '*.java') "$src/Toy.java" "$src/LauncherView.java" "$src/CompanionView.java" "$src/Native.java"
timeout 300 java -Djava.awt.headless=true -cp "$build" io.github.sirbilliams.rollerbot.RenderLauncher "$out"
