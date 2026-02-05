# MaiaScan

MaiaScan is a high-performance memory scanning and manipulation tool designed for reverse engineering and debugging. It leverages modern C++26 features and SIMD acceleration to provide a blazing-fast experience for searching and monitoring virtual memory.

## Key Features

- **SIMD-Accelerated Scanning**: Utilizes AVX2 instructions for high-speed memory searching, with intelligent runtime CPU detection and scalar fallbacks.
- **Cheat Table & Value Freezing**: Save favorite addresses to a cheat table where you can monitor, rename, and "freeze" them (locking the value in the target process).
- **High-Performance Architecture**:
  - **Lock-Free Reads**: The UI uses an Atomic Snapshot pattern (RCU) for 100% wait-free rendering, even during heavy memory updates.
  - **Zero-Allocation Hot Path**: The background monitoring loop operates without heap allocations in the steady state.
- **Iterative Filtering**: Refine results through subsequent scans using comparison logic (Exact, Changed, Unchanged, Increased, Decreased, Increased By, Decreased By).
- **Comprehensive Type Support**: Full support for standard primitive types:
  - Integers: 8, 16, 32, and 64-bit (Signed and Unsigned).
  - Floating Point: Float (32-bit) and Double (64-bit).

## Building the Project

### Prerequisites

- **C++26** compatible compiler (MSVC 19.50+, Clang 18+, or GCC 14+).
- **CMake 3.20** or higher.
- **Python 3.x** (for the unified build script).

### Build & Test

The project includes a unified Python script that handles environment setup, building, and running tests:

```bash
python scripts/build.py
```

To run only the tests using the configured presets:

```bash
ctest --preset windows-release
```

## Usage

1.  **Select Process**: Attach MaiaScan to a running process using the process picker.
2.  **First Scan**:
    - For known values, enter the value and select the type (e.g., Int32).
    - For unknown values, select "Unknown Initial Value" to snapshot the memory state.
3.  **Next Scan**:
    - Change the state in the target application.
    - Select the comparison mode (e.g., "Increased" or "Changed").
    - Click "Next Scan" to filter the candidate list.
4.  **Manage Results**:
    - Double-click a result to add it to your **Cheat Table**.
    - Rename entries, change values, or check the "Active" box to **Freeze** them.

## Disclaimer

Messing with the memory of running processes can cause them to crash or behave unexpectedly. Use this tool responsibly.
