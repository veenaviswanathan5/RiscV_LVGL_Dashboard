#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
command -v sdl2-config >/dev/null || { echo "sdl2-config missing: install libsdl2-dev." >&2;exit 1; }
make -f Makefile.lvgl CROSS="${CROSS:-riscv64-unknown-elf-}" -j "${JOBS:-4}" firmware
make -f Makefile.lvgl emulator
