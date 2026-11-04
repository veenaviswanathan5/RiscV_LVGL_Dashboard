#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
[[ -x obj_lvgl/rv_lvgl && -f build_lvgl/dashboard.bin ]] || { echo "Run bash lvgl_soc/scripts/build_lvgl.sh first." >&2;exit 1; }
exec ./obj_lvgl/rv_lvgl build_lvgl/dashboard.bin --cycles "${LVGL_MAX_CYCLES:-100000000000}"
