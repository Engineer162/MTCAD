#!/bin/sh
set -eu

APP_LIB_DIR="/usr/lib/mtcad"
APP_DATA_DIR="/usr/share/mtcad/assets"

export MTCAD_APP_LIB_DIR="$APP_LIB_DIR"
export MTCAD_APP_DATA_DIR="$APP_DATA_DIR"

exec /usr/libexec/mtcad/mtcad_gui "$@"