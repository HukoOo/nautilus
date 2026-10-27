#!/bin/bash
# Build the forked Nautilus with the Devel profile (app id org.gnome.Nautilus.Devel)
# and run it alongside the system Nautilus without installing anything.
set -e
cd "$(dirname "$0")"

if [ ! -d build ]; then
    meson setup build \
        -Dprofile=Devel \
        -Dtests=none \
        -Ddocs=false \
        -Dpackagekit=false \
        -Dcloudproviders=false \
        -Dintrospection=false
fi

ninja -C build

[ "$1" = "--build-only" ] && exit 0

# Use the fork's GSettings schema (extra zoom levels) instead of the system one.
export GSETTINGS_SCHEMA_DIR="$PWD/build/data${GSETTINGS_SCHEMA_DIR:+:$GSETTINGS_SCHEMA_DIR}"
exec ./build/src/nautilus --new-window "$@"
