# Maiascan

![Language](https://img.shields.io/github/languages/top/marcogmaia/maiascan)
![License](https://img.shields.io/github/license/marcogmaia/maiascan)
![Version](https://img.shields.io/badge/version-0.0.1-blue)

A powerful memory scanner for Windows with an intuitive ImGui interface.

`maiascan` is a sophisticated memory analysis tool that allows you to scan, monitor, and analyze the memory of running processes. Built with modern C++ and featuring a clean ImGui interface, it provides both novice and advanced users with powerful memory inspection capabilities.

## Features

### Current Features

- **Process Selection**: Browse and attach to running Windows processes
- **Memory Scanning**: Perform initial and subsequent memory scans with multiple comparison types
- **Multiple Data Types**: Support for various data types (integers, floats, etc.)
- **Advanced Scan Comparisons**:
  - Exact value matching
  - Greater than/less than comparisons
  - Range-based scanning (between/not between)
  - Change detection (changed/unchanged)
  - Increase/decrease detection
  - Value difference scanning
- **Real-time Results**: Live updating scan results with address and value display
- **Docking Interface**: Modern ImGui docking system for customizable workspace
- **Memory Region Analysis**: Intelligent scanning across process memory regions

### Technical Features

- Modern C++20 codebase with clean architecture
- MVP (Model-View-Presenter) pattern for maintainability
- Comprehensive unit testing with Google Test
- Robust error handling and logging with spdlog
- Cross-platform build system with CMake and vcpkg

## Architecture

The project follows a modular architecture with clear separation of concerns:

```
src/maia/
├── core/           # Core interfaces and types
├── application/    # Application logic and presenters
├── gui/           # ImGui-based user interface
├── scanner/       # Memory scanning implementation
└── console/       # Console/command-line interface
```

### Key Components

- **IMemoryScanner**: Interface for memory scanning operations
- **IProcess**: Abstraction for process attachment and memory access
- **ScanResultModel**: Manages scan results and data flow
- **ProcessSelector**: Handles process discovery and selection
- **ScannerWidget**: ImGui interface for scanning operations

## Building from Source

### Prerequisites

- Windows 10/11
- Visual Studio 2022 or later
- CMake 4.0 or later
- vcpkg package manager
- C++20 compatible compiler

### Build Instructions

1. **Clone the repository**:

   ```bash
   git clone https://github.com/marcogmaia/maiascan.git
   cd maiascan
   ```

2. **Setup vcpkg** (if not already configured):

   ```bash
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   cd ..
   ```

3. **Configure with CMake**:

   ```bash
   cmake --preset=default
   ```

4. **Build the project**:

   ```bash
   cmake --build build
   ```

5. **Run the application**:
   ```bash
   .\build\src\maia\Debug\maia.exe
   ```

### Dependencies

The project uses vcpkg to manage dependencies:

- **imgui**: Immediate Mode GUI
- **glfw3**: Window management
- **glad**: OpenGL loading
- **gtest**: Unit testing framework
- **cli11**: Command line parsing
- **spdlog**: Logging library
- **entt**: Entity-component-system framework
- **nlohmann-json**: JSON parsing
- **fmt**: String formatting

## Usage

### Basic Workflow

1. Launch `maiascan.exe`
2. Select a target process from the process list
3. Choose the data type and scan comparison
4. Enter the value(s) to search for
5. Click "First Scan" to perform initial scan
6. Modify values in the target process
7. Use "Next Scan" to narrow down results
8. Repeat until you find the desired memory address

### Scan Types

- **First Scan**: Initial scan of process memory
- **Next Scan**: Subsequent scans to filter existing results
- **Unknown Value**: Scan without knowing the exact value
- **Changed/Unchanged**: Detect value modifications
- **Increased/Decreased**: Track value changes over time

## Development

### Project Structure

```
maiascan/
├── src/
│   ├── maia/              # Main application
│   └── fakegame/          # Test target application
├── external/              # Build configuration
├── CMakeLists.txt         # Build configuration
├── vcpkg.json            # Dependencies
└── README.md             # This file
```

### Running Tests

```bash
ctest --test-dir build
```

### Code Style

- The project uses `.clang-format` for consistent code formatting
- `.clang-tidy` for static analysis
- Pre-commit hooks for code quality

## ⚠️ Disclaimer

Reading and writing to the memory of running processes can cause them to crash, behave unexpectedly, or trigger anti-cheat systems. Use this tool responsibly and only on applications you have permission to analyze.

This tool is intended for educational purposes, debugging, and legitimate reverse engineering. The authors are not responsible for any misuse or damage caused by this software.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the **GNU General Public License v3.0** - see the [LICENSE](LICENSE) file for details.

### Why GPL v3?

We chose GPL v3 for Maiascan because it aligns with our ethical commitment to the open-source community:

- **Transparency**: All modifications must be shared, ensuring the tool remains transparent and trustworthy
- **Community Protection**: Prevents proprietary forks that could hide malicious features or security issues
- **Patent Protection**: Contributors grant patent licenses, protecting users from patent litigation
- **User Freedom**: Guarantees users can always access, modify, and distribute the software
- **Security Focus**: Critical for a memory analysis tool where transparency is essential

This ensures Maiascan remains a true treasure for the open-source world, where improvements benefit everyone and the tool maintains its integrity for security research, education, and legitimate reverse engineering.

## Acknowledgments

- Built with [Dear ImGui](https://github.com/ocornut/imgui)
- Inspired by memory analysis tools and debuggers
- Thanks to the open-source community for the excellent libraries

## Support

If you encounter any issues or have questions:

- Open an issue on GitHub
- Check the [ROADMAP.md](ROADMAP.md) for planned features
- Review the documentation in the `docs/` directory

---

**Note**: This is a work in progress. Features and APIs are subject to change. Please check the [ROADMAP.md](ROADMAP.md) for upcoming features and improvements.
