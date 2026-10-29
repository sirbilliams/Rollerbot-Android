#!/bin/sh
# Flatpak launcher. The game files are the user's own: on first run it asks for the folder holding
# Rollerbot.exe and copies it into the app's data folder. Settings live in rollerbot.conf.
#   rollerbot-launcher                       run (asks for the folder if none yet)
#   rollerbot-launcher --import /some/dir [--lang french]   copy a game folder in (grant access first:
#                                            flatpak run --filesystem=/some/dir io.github.sirbilliams.Rollerbot --import /some/dir)
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/game"
CONF="${XDG_CONFIG_HOME:-$HOME/.config}/rollerbot.conf"

LANGS="english french german spanish italian"

find_root() {   # print the dir under $1 holding Rollerbot.exe (case-insensitive, 4 levels)
    find "$1" -maxdepth 4 -iname rollerbot.exe 2>/dev/null | head -n 1 | xargs -r -d '\n' dirname
}

# A set's language comes from its files (the loading screens: load1.pcx = english, load1f/g/s/i.pcx = french/german/spanish/italian),
# never from a folder name: detect_lang <dir> prints it (nothing when unknown).
detect_lang() {
    n=$(find "$1" -maxdepth 4 -type f -iname 'load[1-9][fgsi].pcx' 2>/dev/null | head -n 1 | xargs -r -d '\n' basename | tr 'A-Z' 'a-z')
    case "$n" in
        load?f.pcx) echo french; return ;; load?g.pcx) echo german; return ;;
        load?s.pcx) echo spanish; return ;; load?i.pcx) echo italian; return ;;
    esac
    [ -n "$(find "$1" -maxdepth 4 -type f -iname 'load[1-9].pcx' 2>/dev/null | head -n 1)" ] && echo english
}

# game/<lang>/ must really be <lang>: move a mislabelled set to its real slot (a duplicate of a right one is dropped).
repair_labels() {
    for l in $LANGS; do
        [ -d "$DATA/$l" ] || continue
        d=$(detect_lang "$DATA/$l")
        if [ -n "$d" ] && [ "$d" != "$l" ]; then
            echo "language set in $l is really $d: relabelled" >&2
            if [ -e "$DATA/$d" ]; then rm -rf "$DATA/$l"; else mv "$DATA/$l" "$DATA/$d"; fi
        fi
    done
    echo "Rollerbot: language sets: $(for l in $LANGS; do [ -d "$DATA/$l" ] && echo "$l=$(detect_lang "$DATA/$l" || true)"; done | tr '\n' ' ')" >&2
}

# The installed game is $DATA/<language>/ (a whole game per language). The language: RB_LANG (rollerbot.conf), default english.
lang_root() { find_root "$DATA/$1"; }

import_dir() {   # import_dir <folder> <language>: every CodeAndData(<language>) set in it, else the folder itself as <language>
    src="$1"; want="$2"
    [ -d "$src" ] || { echo "Not a folder: $src" >&2; return 1; }
    case " $LANGS " in *" $want "*) ;; *) echo "Unknown language '$want' (one of: $LANGS)" >&2; return 1 ;; esac
    rm -rf "$DATA"; mkdir -p "$DATA"
    have=""
    for l in $LANGS; do
        d=$(find "$src" -maxdepth 4 -type d -iname "CodeAndData($l)" 2>/dev/null | head -n 1)
        [ -n "$d" ] || continue
        mkdir -p "$DATA/$l" && cp -r "$d"/. "$DATA/$l"/ && have="$have $l"
    done
    if [ -z "$have" ]; then      # a plain game folder: the chosen language's set
        root=$(find_root "$src")
        [ -n "$root" ] || { echo "No Rollerbot.exe found under $src" >&2; return 1; }
        real=$(detect_lang "$root"); [ -n "$real" ] || real="$want"
        mkdir -p "$DATA/$real" && cp -r "$root"/. "$DATA/$real"/ || return 1
        echo "Imported $root as $real"
        [ "$real" = "$want" ] || { echo "That folder is $real, not $want." >&2; return 1; }
        return 0
    fi
    echo "Imported:$have"
    case "$have " in *" $want "*) ;; *) echo "$want isn't in that source (it has:$have); pick one with RB_LANG in rollerbot.conf." >&2; return 1 ;; esac
}

if [ "$1" = "--import" ]; then
    lang=english
    [ "$3" = "--lang" ] && lang="$4"
    import_dir "$2" "$lang"; exit $?
fi

[ -d "$DATA" ] && repair_labels
LANG_WANT="${RB_LANG:-english}"
if [ -z "$(lang_root "$LANG_WANT")" ] && [ -z "$(find_root "$DATA")" ]; then
    path=""
    if command -v zenity >/dev/null 2>&1; then
        path=$(zenity --file-selection --directory --title="Select your Rollerbot game folder (with Rollerbot.exe)" 2>/dev/null)
    elif [ -t 0 ]; then
        printf 'Path to your Rollerbot game folder (the one with Rollerbot.exe): '
        read -r path
    fi
    if [ -n "$path" ]; then
        import_dir "$path" "$LANG_WANT" || exit 1
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
RB_LANG=english         # english, french, german, spanish or italian (the sets you imported)
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
LANG_WANT="${RB_LANG:-english}"
RB_DATA="$(lang_root "$LANG_WANT")"
if [ -z "$RB_DATA" ]; then
    echo "Language '$LANG_WANT' isn't installed. Installed: $(ls "$DATA" 2>/dev/null | tr '\n' ' ')(set RB_LANG)" >&2
    exit 1
fi
export RB_DATA
RB_LANGS=""    # every installed set, for the in-game Game language switch
for l in $LANGS; do r=$(lang_root "$l"); [ -n "$r" ] && RB_LANGS="${RB_LANGS:+$RB_LANGS;}$l=$r"; done
export RB_LANGS RB_LANG="$LANG_WANT"
cd "$RB_DATA" || exit 1
exec /app/bin/rollerbot "$@"
