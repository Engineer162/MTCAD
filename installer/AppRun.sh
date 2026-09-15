#!/bin/sh
set -eu

HERE="$(dirname "$(readlink -f "$0")")"

export MTCAD_APP_LIB_DIR="$HERE/usr/lib/mtcad"
export MTCAD_APP_DATA_DIR="$HERE/usr/share/mtcad/assets"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec "$HERE/usr/libexec/mtcad/mtcad" "$@"