# MaiaScan Agent Guidelines

This document provides essential context, standards, and instructions for AI agents working on the MaiaScan codebase.

## 1. Build and Test Automation

We use a unified Python script to handle the build lifecycle. Do not invoke `cmake` directly unless debugging build system issues.

### Core Build Commands

| Action           | Command                                 | Description                              |
| ---------------- | --------------------------------------- | ---------------------------------------- |
| **Build & Test** | `python scripts/build.py`               | Configures, builds, and runs all tests.  |
| **Clean Build**  | `python scripts/build.py --clean`       | Wipes `out/` directory and starts fresh. |
| **Build Only**   | `python scripts/build.py --skip-tests`  | Compiles without running the test suite. |
| **Reconfigure**  | `python scripts/build.py --reconfigure` | Forces CMake re-configuration.           |

### Running and Debugging Tests

Tests are built using GoogleTest (GTest) and managed via CTest.

- **List all test targets:**
  ```bash
  ctest --preset windows-debug -N
  ```
- **Run specific tests by name (Regex):**
  ```bash
  # Runs all tests in the SimdScanner suite
  ctest --preset windows-debug -R SimdScanner --output-on-failure
  ```
- **Run a specific test executable directly:**
  Test executables are located in `out/build/windows-debug/src/maia/...`. You can run them directly to use GTest flags like `--gtest_filter`.
  ```bash
  ./out/build/windows-debug/src/maia/core/simd_scanner_test.exe --gtest_filter=SimdScannerTest.ExactMatch
  ```

### Linting & Formatting

- **Style:** We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
- **Indentation:** 2 spaces. No tabs.
- **Line Length:** 80-100 characters.
- **Formatting Tool:** All code must be formatted with `clang-format` before committing.
  ```bash
  clang-format -i src/maia/path/to/file.cpp
  ```

## 2. Development Workflow (TDD)

We strictly follow **Test-Driven Development (TDD)**. Do not write implementation code without a failing test case.

### Core Philosophy

- **Test Public API Only:** Verify behavior through public interfaces. Never test private members or internal implementation details.
- **Design Feedback:** Difficulty in writing a test indicates **poor interface design**. Refactor the API rather than forcing a brittle test.
- **Isolation:** Business logic must be decoupled from the OS. Use mocks (e.g., `FakeProcess`) to simulate system state.

### The Cycle

1.  **Red:** Create a reproduction case (bugs) or a requirement case (features) that fails.
2.  **Green:** Write minimal code to satisfy the test.
3.  **Refactor:** Clean up the code while maintaining passing tests.

## 3. Code Style and Architectural Guidelines

### General Principles

- **Safety First:** As a memory scanner, we deal with volatile external state. Validate all process handles, memory addresses, and buffer sizes.
- **Flat Structure:** Avoid deep nesting. Use **guard clauses** (`if (!condition) return;`) to keep the "happy path" at the root indentation level.
- **Modern C++:** Use C++20 features where they improve clarity and safety.
  - Prefer `std::span<std::byte>` for memory buffers over `void*` and size.
  - Use `std::optional<T>` for functions that can fail to return a value.
  - Use `concepts` to constrain template parameters.

### Naming Conventions

- **Files:** `snake_case.cpp` / `snake_case.h`.
- **Types/Classes:** `PascalCase` (e.g., `ScanResultModel`).
- **Functions:** `PascalCase` (e.g., `ReadMemory`).
- **Variables:** `snake_case` (e.g., `buffer_size`).
- **Member Variables:** `snake_case_` (with trailing underscore).
- **Constants:** `kPascalCase` (e.g., `kMaxBatchSize`).
- **Namespaces:** `snake_case` (e.g., `maia::core`).

### Header Management

- **Pragma:** Use `#pragma once` in all headers.
- **Include Order:**
  1.  Related header (e.g., `process.h` in `process.cpp`).
  2.  C system headers (e.g., `<Windows.h>`).
  3.  C++ standard library headers (e.g., `<vector>`).
  4.  Third-party libraries (e.g., `<imgui.h>`, `<entt/entt.hpp>`).
  5.  Project headers using full paths from `src` (e.g., `#include "maia/core/process.h"`).

### Error Handling & Logging

- **Return Values:** Prefer returning `bool` or `std::optional` for expected runtime failures (e.g., "process not found").
- **Assertions:** Use `maia/assert.h` (`Assert(condition)`) for internal logic invariants that should never be violated.
- **Logging:** Use the macros in `maia/logging.h` for all telemetry.
  - `LogInfo("Message {}", arg);`
  - `LogWarning(...)`, `LogError(...)`, `LogDebug(...)`.
- **Exceptions:** Avoid exceptions for flow control. Use them only for truly exceptional, unrecoverable system errors if necessary.

### Memory & Resource Management

- **RAII:** Always use RAII for resource management (handles, memory, locks).
- **Smart Pointers:** Use `std::unique_ptr` for ownership. Avoid `std::shared_ptr` unless shared ownership is strictly required.
- **Casts:** Never use C-style casts. Use `static_cast`, `reinterpret_cast`, or `std::bit_cast`.

## 4. Project Architecture

- **Core (`src/maia/core`):** Platform-independent abstractions and heavy-lifting logic (Process management, Scanning algorithms).
- **MMEM (`src/maia/mmem`):** Low-level, platform-specific memory manipulation.
- **Application (`src/maia/application`):** The "glue" and business logic. Uses the **Presenter** pattern to coordinate between Models and Views.
- **GUI (`src/maia/gui`):** ImGui-based views and widgets.
- **Communication:** We use `EnTT`'s signal system (`entt::sigh`, `entt::sink`) for decoupled communication between components.

## 5. Dependencies

The project uses `vcpkg` in manifest mode. Key dependencies include:

- **EnTT:** For the signal/event system.
- **ImGui:** For the graphical user interface.
- **spdlog:** For high-performance logging.
- **fmt:** For string formatting.
- **GTest:** For unit and integration testing.

## 6. Environment Requirements

- **Primary Platform:** Windows 10/11.
- **Compiler:** MSVC (Visual Studio 2022+) with C++20 support.
- **Build Tools:** Python 3.x, CMake 3.20+, Ninja.

---

_Generated by MaiaScan AI Assistant_
