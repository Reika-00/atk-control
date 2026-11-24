#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
APPDIR="${APPDIR:-$ROOT/AppDir}"
TOOLS_DIR="${APPIMAGE_TOOLS:-/tmp/appimage-tools}"

LINUXDEPLOY="${LINUXDEPLOY:-$TOOLS_DIR/linuxdeploy}"
LINUXDEPLOY_QT="${LINUXDEPLOY_QT:-$TOOLS_DIR/linuxdeploy-plugin-qt}"
APPIMAGETOOL="${APPIMAGETOOL:-$TOOLS_DIR/appimagetool}"
REAL_QMAKE="${REAL_QMAKE:-$(command -v qmake6 || command -v qmake || true)}"

if [[ ! -x "$LINUXDEPLOY" ]]; then
    echo "linuxdeploy not found: $LINUXDEPLOY" >&2
    echo "Set APPIMAGE_TOOLS or LINUXDEPLOY to the directory containing the AppImage tools." >&2
    exit 1
fi
if [[ ! -x "$LINUXDEPLOY_QT" ]]; then
    echo "linuxdeploy-plugin-qt not found: $LINUXDEPLOY_QT" >&2
    exit 1
fi
if [[ ! -x "$APPIMAGETOOL" ]]; then
    echo "appimagetool not found: $APPIMAGETOOL" >&2
    exit 1
fi
if [[ -z "$REAL_QMAKE" ]]; then
    echo "qmake6 or qmake was not found in PATH." >&2
    exit 1
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

QT_PLUGIN_SOURCE="$("$REAL_QMAKE" -query QT_INSTALL_PLUGINS)"
PLUGIN_DIR="$BUILD_DIR/qt-plugins"
QMAKE_WRAPPER="$BUILD_DIR/qmake-wrapper"

rm -rf "$PLUGIN_DIR"
mkdir -p "$PLUGIN_DIR/platforms" "$PLUGIN_DIR/platforminputcontexts"

link_plugin() {
    local relative="$1"
    if [[ -f "$QT_PLUGIN_SOURCE/$relative" ]]; then
        ln -sf "$QT_PLUGIN_SOURCE/$relative" "$PLUGIN_DIR/$relative"
    fi
}

link_plugin platforms/libqxcb.so
link_plugin platforms/libqwayland.so
link_plugin platforms/libqoffscreen.so
link_plugin platforminputcontexts/libcomposeplatforminputcontextplugin.so

cat > "$QMAKE_WRAPPER" <<EOF
#!/bin/sh
if [ "\$1" = "-query" ]; then
    if [ -n "\$2" ]; then
        if [ "\$2" = "QT_INSTALL_PLUGINS" ]; then
            echo "$PLUGIN_DIR"
        else
            exec "$REAL_QMAKE" "\$@"
        fi
    else
        "$REAL_QMAKE" -query | sed 's#^QT_INSTALL_PLUGINS:.*#QT_INSTALL_PLUGINS:$PLUGIN_DIR#'
    fi
    exit 0
fi
exec "$REAL_QMAKE" "\$@"
EOF
chmod +x "$QMAKE_WRAPPER"

rm -rf "$APPDIR"
install -Dm755 "$BUILD_DIR/atk-control" "$APPDIR/usr/bin/atk-control"
install -Dm644 "$ROOT/packaging/atk-control.desktop" \
    "$APPDIR/usr/share/applications/atk-control.desktop"
install -Dm644 "$ROOT/packaging/atk-control.appdata.xml" \
    "$APPDIR/usr/share/metainfo/atk-control.appdata.xml"
install -Dm644 "$ROOT/LICENSE" "$APPDIR/usr/share/licenses/atk-control/LICENSE"
install -Dm644 "$ROOT/packaging/icons/atk-control.svg" \
    "$APPDIR/usr/share/icons/hicolor/scalable/apps/atk-control.svg"
install -Dm644 "$ROOT/packaging/icons/atk-control.png" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps/atk-control.png"
install -Dm644 "$ROOT/packaging/icons/atk-control.png" "$APPDIR/atk-control.png"

export QMAKE="$QMAKE_WRAPPER"
export PATH="$TOOLS_DIR:$PATH"
export ARCH="${ARCH:-x86_64}"
export APPIMAGE_EXTRACT_AND_RUN=1
export NO_STRIP="${NO_STRIP:-1}"

cd "$ROOT"
"$LINUXDEPLOY" --appdir "$APPDIR" --plugin qt

copy_plugin() {
    local relative="$1"
    if [[ -f "$QT_PLUGIN_SOURCE/$relative" ]]; then
        install -Dm755 "$QT_PLUGIN_SOURCE/$relative" "$APPDIR/usr/plugins/$relative"
    fi
}

copy_plugin platforms/libqwayland.so
copy_plugin platforms/libqoffscreen.so
copy_plugin wayland-shell-integration/libxdg-shell.so
copy_plugin wayland-graphics-integration-client/libqt-plugin-wayland-egl.so

while IFS= read -r -d '' plugin; do
    "$LINUXDEPLOY" --appdir "$APPDIR" --deploy-deps-only "$plugin"
done < <(find "$APPDIR/usr/plugins" -type f -name '*.so' -print0)

rm -f "$APPDIR/AppRun"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/atk-control" "$@"
EOF
chmod +x "$APPDIR/AppRun"

TARGET="$ROOT/atk-control-1.0.0-x86_64.AppImage"
rm -f "$TARGET"
APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" --no-appstream "$APPDIR" "$TARGET"

printf '\nCreated: %s\n' "$TARGET"
