#!/bin/sh
set -eu

HERE="$(dirname "$(readlink -f "$0")")"

export MTCAD_APP_LIB_DIR="$HERE/usr/lib/mtcad"
export MTCAD_APP_DATA_DIR="$HERE/usr/share/mtcad/assets"

exec "$HERE/usr/libexec/mtcad/mtcad_gui" "$@"