#!/bin/sh
set -eu

APP_DIR=/usr/lib/mtcad

cd "$APP_DIR"
exec "$APP_DIR/mtcad_gui" "$@"
