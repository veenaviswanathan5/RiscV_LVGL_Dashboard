#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../.."
for f in Makefile Makefile.lvgl firmware/start.S firmware/link.ld rtl/topCPU.sv; do
    [[ -f "$f" ]] || { echo "Missing $f. Extract the addon into the restored RV_SoC_LVGL project." >&2; exit 1; }
done
prefix="${CROSS:-riscv64-unknown-elf-}"
for cmd in make cc g++ verilator "${prefix}gcc" "${prefix}objcopy"; do
    command -v "$cmd" >/dev/null || { echo "Missing tool: $cmd" >&2; exit 1; }
done
mkdir -p build_lvgl
{
    verilator --version
    "${prefix}gcc" --version
    g++ --version
} > build_lvgl/tool_versions.txt
echo "1/5: Strict lint and baseline smoke tests"
verilator --lint-only -Wall --top-module topCPU rtl/*.sv
make host-test
make CROSS="$prefix" test
echo "2/5: Extended host MMIO and real-LVGL native tests"
make -f Makefile.lvgl -j "${JOBS:-4}" native-test host-test
echo "3/5: Cross-compile dashboard and self-test"
make -f Makefile.lvgl CROSS="$prefix" -j "${JOBS:-4}" firmware
echo "4/5: Build LVGL headless host"
make -f Makefile.lvgl headless
echo "5/5: Execute LVGL on your RV32 CPU"
./obj_lvgl_headless/rv_lvgl build_lvgl/dashboard_test.bin --headless --progress --cycles 500000000 --dump-ppm build_lvgl/rv32-selftest.ppm
echo "All LVGL automated checks passed. Build the SDL host and launch next."
