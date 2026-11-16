#!/bin/sh
# Build the macOS reference renderer against ANGLE (GLES 2.0 + EGL). ANGLE ships with
# Chromium-based apps; ANGLE_DIR must hold libEGL.dylib and libGLESv2.dylib.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd)
NANO=${NANO:-/Users/jwalr/nano-gles2}
ANGLE_DIR=${ANGLE_DIR:?set ANGLE_DIR to a directory with libEGL.dylib and libGLESv2.dylib}
mkdir -p "$HERE/build"
cc -c -O1 -w "$ROOT/engine/src/libs/tinf/tinflate.c" -o "$HERE/build/tinflate.o"
c++ -std=c++11 -O1 -w -D_OS_NANO7 -D_GAPI_GLES2 -DUSE_ATLAS_RGBA16 -DNO_SOUND -DNO_VIDEO $HOST_CFLAGS \
    -I"$ROOT/engine/src" -I"$NANO/NanoOS/runtime/gl2/std" \
    "$HERE/host_ref.cpp" "$HERE/build/tinflate.o" \
    -L"$ANGLE_DIR" -lEGL -lGLESv2 -Wl,-headerpad_max_install_names -Wl,-rpath,"$ANGLE_DIR" -o "${HOST_OUT:=$HERE/build/host_ref}"
# ANGLE builds use relative install names (./libEGL.dylib): resolve them through the rpath
install_name_tool -change ./libEGL.dylib @rpath/libEGL.dylib -change ./libGLESv2.dylib @rpath/libGLESv2.dylib "$HOST_OUT" 2>/dev/null
codesign -f -s - "$HOST_OUT" 2>/dev/null || true
echo "built $HOST_OUT"
