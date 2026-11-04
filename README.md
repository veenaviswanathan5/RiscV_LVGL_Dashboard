# RV32IM SoC with LVGL Hardware Dashboard

A custom 32-bit RISC-V System-on-Chip (SoC) designed from scratch in SystemVerilog, demonstrating cycle-accurate hardware-software co-simulation. This project integrates a 5-stage pipelined RV32IM processor with an AXI4-Lite bus and memory-mapped peripherals to drive a bare-metal graphical user interface using the LVGL library.

## Project Demonstration



## Overview
The goal of this project was to bridge low-level digital logic design with embedded C software engineering. Rather than running a standard operating system, the CPU directly executes bare-metal C firmware to render a 320x240 UI and process real-time inputs. The RTL is compiled using Verilator, wrapped in a C++ host, and connected to SDL2 for windowing and keyboard event dispatching on WSL2/Ubuntu.

## Key Features & Architecture

* **Custom RV32IM Processor:** A 5-stage pipelined architecture featuring hardware hazard detection, data forwarding multiplexers, and an AXI4-Lite bus interconnect.
* **Bare-Metal LVGL GUI:** A lightweight, flat-theme dashboard built with LVGL 9.2.2, configured to keep VRAM writes minimal for high simulation performance.
* **Memory-Mapped I/O (MMIO) Integration:** The dashboard interacts directly with custom hardware addresses to prove real-time hardware-software synchronization:
  * **Video RAM:** 320x240 framebuffer mapped at `0x20000000` to isolate heavy display traffic from general logic.
  * **Live Telemetry:** The GUI's system monitor continuously polls the hardware timer mapped at `0x3000000C`.
  * **Virtual Actuation:** UI switches and duty sliders write command values down to custom MMIO actuator addresses (`0x30000040`).
  * **Hardware Event Logging:** Laptop keystrokes are intercepted by SDL2 and routed into a custom hardware keyboard-event queue at `0x30000010`, allowing UI navigation without a mouse.

## Technical Stack
* **Hardware Design:** SystemVerilog, AXI4-Lite
* **Verification & Simulation:** Verilator, SDL2, C++ Wrapper
* **Firmware:** Embedded C, LVGL (Light and Versatile Graphics Library)
* **Environment:** WSL2 (Ubuntu), GNU Make, GCC Cross-Compiler for RISC-V
