# RV32 LVGL dashboard — complete build and acceptance guide

This package extends your restored RV_SoC_LVGL timer baseline. It supplies real
LVGL 9.2.2, the GUI firmware, a matching AXI/MMIO/SDL host, separate build rules,
and automated checks. No manual source edits are needed.

Your completed Tetris application remains in RV_SoC. Its successful gameplay
and smoke tests establish the application milestone, not full RV32IM compliance.
Keep RV_SoC_timer_baseline.tar.gz and your completed Tetris archive unchanged.

## 1. What the GUI does

A compact 320x240 interface has three tabs:

- **Monitor:** elapsed host milliseconds received through AXI, uptime, a
  ten-second timer sweep, cumulative AXI reads/writes, VRAM word writes, and
  LVGL flush-area count. These are real counters, not synthetic CPU utilization.
- **Control:** a switch and duty slider write MMIO commands; the same registers
  are read back and a separate acknowledgement counter increments on accepted
  commands. This is a virtual host latch, not a physical LED/motor/PWM signal.
- **Logs:** a six-entry scrolling event log, a 32-character note entry, a submit
  button, a clear button, and a freeze-monitor switch. Navigation keys are
  consumed as controls; printable ASCII enters the focused note field.

Useful additions are bounded memory, input-queue overflow detection, keyboard
shortcuts, explicit command acknowledgement, a timer probe, screenshots from
the actual renderer, and a self-test that runs inside the custom CPU.

## 2. Corrected memory map

Your proposed keyboard/timer addresses came from the earlier, different slave.
The working baseline uses UART at +0, keys at +4, exit at +8 and timer at +12.
This package preserves those registers and adds new ones:

| Address | Access | Meaning |
|---|---|---|
| 0x00000000–0x000FFFFF | R | ROM and reset vector zero |
| 0x10000000–0x100FFFFF | RW | 1 MiB RAM |
| 0x20000000–0x2004AFFF | RW | 320x240 ARGB8888 framebuffer |
| 0x30000000 | W | Console low byte |
| 0x30000004 | R | Original six-key bitmask, retained for compatibility |
| 0x30000008 | W | Firmware exit status |
| 0x3000000C | R | Elapsed host milliseconds |
| 0x30000010 | R | Queued keyboard-event count |
| 0x30000014 | R | Pop one keyboard event, or zero if empty |
| 0x30000018 | R | Dropped keyboard-event count |
| 0x3000001C | R | Low 32 bits of simulated cycle count |
| 0x30000020 | R | Cumulative data-bus reads |
| 0x30000024 | R | Cumulative data-bus writes |
| 0x30000028 | R | Cumulative VRAM word writes |
| 0x30000040 | RW | Virtual output enable, bit zero |
| 0x30000044 | RW | Duty command, clamped to 0–100 |
| 0x30000048 | R | Accepted actuator-write count |

The input FIFO holds 64 key-press events; key releases are synthesized for LVGL.
SDL key repeat supplies repeated presses. Overflow drops new events and counts
them. This prevents the GUI's six-key legacy register from pretending to carry
arbitrary text. ASCII notes are supported; full Unicode/IME input is not.
Counter values wrap modulo 2^32. The timer is host wall time from emulator start,
not a hardware timer interrupt or a declared simulated CPU clock frequency.

## 3. Install

Put RV_SoC_LVGL_dashboard.zip in your Ubuntu home folder, then:

```bash
cd ~/RV_SoC_LVGL
python3 -m zipfile -e ~/RV_SoC_LVGL_dashboard.zip .
```

The ZIP adds Makefile.lvgl, this guide, validation notes, and lvgl_soc/.
It does not overwrite the baseline RTL, Makefile, main.cpp, axi_slave.h,
firmware/start.S, or firmware/link.ld. All LVGL host replacements live under
lvgl_soc/host and are selected by the new build target automatically.

Bundled LVGL is pinned to 9.2.2 with its license and source provenance. There
is no GitHub download or Python package installation in the build scripts.
Your existing tools are sufficient: build-essential, Verilator, SDL2 development
files, and the RISC-V bare-metal compiler/binutils. A clean first build compiles
many LVGL source files; disabled components compile to empty objects.

## 4. Run automatic verification first

```bash
cd ~/RV_SoC_LVGL
bash lvgl_soc/scripts/verify_lvgl.sh
```

The script stops at the first failure and performs:

1. Strict RTL lint and your existing AXI/smoke tests.
2. Extended AXI slave and MMIO tests.
3. Native execution of real LVGL with framebuffer and keyboard-queue tests.
4. Cross-compilation of playable firmware and firmware self-test.
5. A headless Verilator build and execution of the LVGL self-test on your CPU.

The native test generates build_lvgl/monitor.ppm, control.ppm and logs.ppm.
The CPU self-test generates build_lvgl/rv32-selftest.ppm on completion.
Tool versions are written to build_lvgl/tool_versions.txt.
PPM files are ordinary images supported by many viewers; the package also
includes PNG previews from native rendering under lvgl_soc/previews/.

Expected ending:

```text
LVGL RV32 self-test PASS
Cycles: <depends on host timing>
Firmware exit: 0
All LVGL automated checks passed. Build the SDL host and launch next.
```

Progress lines report a fetch PC every ten million cycles. This PC is a debug
location, not an instruction-retirement count. Unlike the old 493-cycle test,
LVGL initialization and three-tab rendering are substantial workloads. Allow
the test to finish. A cycle-limit stop is not a passing result.

To preserve the exact script exit code while recording output:

```bash
set -o pipefail
bash lvgl_soc/scripts/verify_lvgl.sh 2>&1 | tee lvgl_verification.log
```

If a failure occurs, send the full log. Do not silence it or edit code speculatively.

## 5. Build and launch the interactive GUI

```bash
bash lvgl_soc/scripts/build_lvgl.sh
bash lvgl_soc/scripts/run_lvgl.sh
```

Build output: build_lvgl/dashboard.bin. Host: obj_lvgl/rv_lvgl.
Click the window to give it keyboard focus. No mouse interaction is implemented
inside the guest GUI.

| Key | Action |
|---|---|
| F1 / F2 / F3 | Jump to Monitor / Control / Logs |
| Tab / Shift+Tab | Next / previous focusable object on the active tab |
| Enter | Activate focused button/switch; submit a one-line note |
| Left / Right | Adjust a focused slider; move caret in note text |
| Up / Down | Passed to the focused LVGL widget |
| Printable ASCII | Type into the focused note field |
| Backspace | Delete in note field |
| Space | Activate a control, or insert a space when note is focused |
| Escape | Ask the guest to exit successfully |
| Window close | Stop the host |

Tab changes rebuild the focus group, so hidden controls do not receive input.
F2 initially focuses the switch; press Tab once to reach the slider.
F3 initially focuses the note field. The scrolling log is read-only and is not
part of keyboard focus. Press Enter to submit the note and clear the input.
The Freeze switch freezes monitor updates only; the underlying timer continues.
Changing controls or submitting notes appends to the log. Oldest entries are
removed after six entries. Log text is not persisted across emulator sessions.

## 6. Performance choices and limits

- All widget logic, software drawing and pixel stores execute in RV32 firmware.
- SDL displays VRAM; it does not run the guest GUI logic.
- XRGB8888 rendering is converted to opaque ARGB on the final framebuffer store.
- One aligned 320x12-pixel draw buffer uses 15,360 bytes of RAM.
- LVGL's built-in allocator has a fixed 128 KiB pool; no guest libc heap is used.
- Null/allocation assertions remain enabled. Full object-tree validity scans
  are disabled because they are too expensive for this processor model.
- Fonts: only Montserrat 12; no image assets, GPU, filesystem or OS support.
- Monitor refresh is 500 ms, not every simulated CPU cycle.
- LVGL refresh target is 50 ms; input and rendering may be slower if simulation
  throughput cannot keep up. A small framebuffer reduces bus traffic but does
  not guarantee smooth real-time animation.
- Tab switches explicitly invalidate the screen to keep all captions visible.
- The bounded log is replaced in bulk, avoiding costly per-character layout.
- Tab transitions and switches use no transition animation; simple styling
  keeps the first project focused on functional integration.
- CPU architectural exceptions and write-error traps are not added. Invalid
  host memory accesses stop simulation with an error.
- This is a practical embedded dashboard, not a complete CPU compliance test.

The new host retains independent AW/W buffering, pre-edge AXI sampling,
byte-lane support, and held responses. Reads that pop a key event happen once
when accepting the read address, so response backpressure cannot consume extra
keys. Actuator registers require full-word stores; their effects occur once
when buffered address and data are paired.

## 7. Manual acceptance and a strong demonstration

After automated verification, demonstrate these in order:

1. Monitor: uptime increases, the sweep moves, and bus counters change.
2. Control: toggle the output and adjust duty; readback and acknowledgement
   count respond. Explain that these are virtual peripheral registers.
3. Logs: the control events appear. Enter a short note and submit it.
4. Toggle Freeze, return to Monitor, verify it reports paused updates, then
   resume and observe the timer catch up.
5. Clear the log. Switch tabs using shortcuts and keyboard focus using Tab.
6. Escape exits with status zero; restart the GUI to verify a clean boot.

The project is complete as an application demonstrator when strict lint,
baseline tests, new host/native/CPU tests, and this acceptance pass are recorded.
For a presentation, include an architecture diagram, corrected memory map,
three screenshots, a short video, dependency versions, and validation results.
Do not label the timer bar as CPU utilization or the virtual latch as a physical
actuator. Those distinctions make the demonstration technically credible.

## 8. Archive and reproduce

```bash
cd ~/RV_SoC_LVGL
tar --exclude=obj_dir --exclude=obj_headless --exclude=obj_lvgl \
    --exclude=obj_lvgl_headless --exclude=build_lvgl --exclude=rtl_before_cleanup \
    --exclude='*.tar.gz' --exclude='*.zip' \
    -czf ../RV_SoC_LVGL_complete.tar.gz .
```

Extract into an empty directory and run the same verify/build/run scripts to
confirm reproducibility. Keep your Tetris and timer-baseline archives separately.
Use an alternate toolchain prefix via CROSS=your-prefix- and limit build jobs
with JOBS=2 if memory is tight. No source changes are required for either option.

## Files and future extensions

- Makefile.lvgl: separate native, firmware, headless and SDL builds.
- lvgl_soc/app/: GUI, startup entry into the GUI, MMIO platform, minimal runtime.
- lvgl_soc/host/: extended memory model, keyboard FIFO and SDL wrapper.
- lvgl_soc/config/: LVGL configuration and shared MMIO/key definitions.
- lvgl_soc/tests/: host peripheral test and real LVGL native integration test.
- lvgl_soc/scripts/: verify/build/run commands.
- lvgl_soc/vendor/lvgl/: pinned upstream source and license.

Good later extensions are a simulated actuator model with measured feedback,
a deterministic simulated-time mode for replay, and randomized CPU AXI delay
tests. Keep those separate from the acceptance scope above.
