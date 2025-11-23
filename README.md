# MaiaScan

MaiaScan is a high-performance memory scanning and manipulation tool designed
for reverse engineering and debugging. It functions similarly to tools like
Cheat Engine, allowing users to search for values within the virtual memory
space of a target process, filter results based on state changes, and monitor
values in real-time.

## Features

- **Memory Scanning**: Supports comprehensive memory searches including "Exact
  Value" and "Unknown Initial Value" scans.
- **Iterative Filtering**: Refines results through subsequent scans using
  comparison logic (Changed, Unchanged, Increased, Decreased).
- **Type Support**: Full support for standard primitive types:
  - Integers: 8, 16, 32, and 64-bit (Signed and Unsigned).
  - Floating Point: Float (32-bit) and Double (64-bit).
- **Real-time Monitoring**: Auto-update capability to refresh values for found
  addresses, allowing dynamic observation of memory changes.

## Building the Project

### Prerequisites

- C++23 compatible compiler (MSVC, Clang, or GCC).
- CMake 3.20 or higher.

## Usage

1.  **Select Process**: Attach MaiaScan to a running process.
2.  **First Scan**:
    - For known values, enter the value and select the type (e.g., Int32).
    - For unknown values, select "Unknown Initial Value" to snapshot the memory
      state.
3.  **Next Scan**:
    - Change the state in the target application.
    - Select the comparison mode (e.g., "Increased" or "Changed").
    - Click "Next Scan" to filter the candidate list.
4.  **Monitor**: Enable "Auto Update" to watch selected memory addresses change
    in real-time.

## Disclaimer

Reading the memory of running processes can cause them to crash or behave
unexpectedly. Use this tool responsibly.
