# MaiaScan

A fast memory scanner for Windows and Linux.

> Find. Monitor. Freeze.

## What is MaiaScan?

MaiaScan helps you search and monitor values in a process's memory. Whether you're debugging an application, reverse engineering a game, or building a trainer.

It's designed to be fast and responsive even when scanning through gigabytes of memory.

Currently in active development.

## Features

- **Fast searches** - SIMD-accelerated scanning finds values in seconds
- **Freeze values** - Lock memory addresses to keep values steady (health, ammo, currency)
- **Iterative filtering** - Scan, change something, scan again - find exactly what you're looking for
- **Type support** - Integers (8/16/32/64-bit), floats, doubles

## Quick Start

1. Run MaiaScan
2. Select a process from the list
3. Enter a value (or scan for unknown values)
4. Double-click results to add them to your cheat table

## What's Coming

MaiaScan is actively being developed. Here's what's on the roadmap:

- Advanced search (regex, AOB/signature scanning, save/load results)
- Memory analysis tools (hex editor, disassembler, structure viewer)
- Injection & modding (code injection, trampoline hooks, ASM scripts)
- Full GUI, integrated debugger, Python scripting API

## Disclaimer

⚠️ Modifying process memory can cause crashes. Use responsibly.
