#!/bin/sh
# Flatpak launcher. The game files are the user's own: on first run it asks for the folder holding
# Rollerbot.exe and copies it into the app's data folder. Settings live in rollerbot.conf.
#   rollerbot-launcher                       run (asks for the folder if none yet)
#   rollerbot-launcher --import /some/dir    copy a game folder in (grant access first:
#                                            flatpak run --filesystem=/some/dir io.github.sirbilliams.Rollerbot --import /some/dir)
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/game"
CONF="${XDG_CONFIG_HOME:-$HOME/.config}/rollerbot.conf"

find_root() {   # print the dir under $1 holding Rollerbot.exe (case-insensitive, 4 levels)
    find "$1" -maxdepth 4 -iname rollerbot.exe 2>/dev/null | head -n 1 | xargs -r -d '\n' dirname
}

import_dir() {
    src="$1"
    [ -d "$src" ] || { echo "Not a folder: $src" >&2; return 1; }
    root=$(find_root "$src")
    [ -n "$root" ] || { echo "No Rollerbot.exe found under $src" >&2; return 1; }
    rm -rf "$DATA"; mkdir -p "$DATA"
    cp -r "$root"/. "$DATA"/ && echo "Imported $root"
}

if [ "$1" = "--import" ]; then import_dir "$2"; exit $?; fi

if [ -z "$(find_root "$DATA")" ]; then
    path=""
    if command -v zenity >/dev/null 2>&1; then
        path=$(zenity --file-selection --directory --title="Select your Rollerbot game folder (with Rollerbot.exe)" 2>/dev/null)
    elif [ -t 0 ]; then
        printf 'Path to your Rollerbot game folder (the one with Rollerbot.exe): '
        read -r path
    fi
    if [ -n "$path" ]; then
        import_dir "$path" || exit 1
    else
        echo "No game files. Bring your own copy, then run:" >&2
        echo "  flatpak run --filesystem=/path/to/game io.github.sirbilliams.Rollerbot --import /path/to/game" >&2
        exit 1
    fi
fi

if [ ! -f "$CONF" ]; then
    mkdir -p "$(dirname "$CONF")"
    cat > "$CONF" <<'CFG'
# Rollerbot (desktop) settings. Edit and restart.
RB_FULLSCREEN=1         # 1 = fullscreen, 0 = window (RB_SIZE=1280x720 sets its size)
RB_WIDESCREEN=fill      # fill, panels, stretch or off (original 4:3)
CFG
fi
# each NAME=value line, unless NAME is already set (flatpak run --env=... wins)
while IFS= read -r line; do
    line=${line%%#*}
    case "$line" in *=*) ;; *) continue ;; esac
    name=$(printf '%s' "${line%%=*}" | tr -d ' ')
    value=$(printf '%s' "${line#*=}" | sed 's/[[:space:]]*$//')
    eval "[ -n \"\${$name+x}\" ]" || export "$name=$value"
done < "$CONF"
export RB_DATA="$(find_root "$DATA")"
cd "$RB_DATA" || exit 1
exec /app/bin/rollerbot "$@"
