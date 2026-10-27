#!/bin/bash
# Run the already-built fork (no rebuild). Used by the dock launcher
# ~/.local/share/applications/org.gnome.NautilusDevel.desktop.
cd "$(dirname "$(readlink -f "$0")")"

# Use the fork's GSettings schema (extra zoom levels, icon size, font scale).
export GSETTINGS_SCHEMA_DIR="$PWD/build/data${GSETTINGS_SCHEMA_DIR:+:$GSETTINGS_SCHEMA_DIR}"
# GTK unsets the launcher's startup id as soon as it is loaded, so keep a copy
# for nautilus_application_handle_local_options() to forward to the running
# instance. Without it, windows opened that way end up behind other windows.
export NAUTILUS_LAUNCH_STARTUP_ID="${XDG_ACTIVATION_TOKEN:-$DESKTOP_STARTUP_ID}"
# Keep a log, since launches from the dock have no terminal.
LOG="${XDG_CACHE_HOME:-$HOME/.cache}/nautilus-dev.log"
echo "=== $(date '+%F %T') pid $$ launch: $*" >> "$LOG"
exec ./build/src/nautilus "$@" >> "$LOG" 2>&1
