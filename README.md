# Boats: Networked Battleship Game

[![Build All Platforms](https://github.com/WitcherTro/boats/actions/workflows/build_multiplatform.yml/badge.svg)](https://github.com/WitcherTro/boats/actions/workflows/build_multiplatform.yml)

A robust, cross-platform multiplayer implementation of the classic Battleship game, written in C. This project features a custom binary protocol, a multi-threaded server, and two distinct client interfaces.

## 🌟 Features

*   **Multiplayer**: Real-time 1v1 gameplay over TCP/IP.
*   **Dual Interface**:
    *   **CLI Client**: Lightweight, terminal-based interface for minimalists.
    *   **GUI Client**: Interactive graphical interface powered by [Raylib](https://www.raylib.com/).
    *   **Web Client**: Browser-based interface (requires Node.js gateway).
*   **Chat System**: Integrated real-time messaging between opponents.
*   **Cross-Platform**: Fully supported on **Windows** (MinGW), **Linux**, and **macOS** (Intel & Apple Silicon).
*   **Lobby System**: Automated matchmaking queue.

## 🛠️ Build & Installation

### Prerequisites
*   **CMake** (3.25 or higher)
*   **C Compiler** (GCC, Clang, or MinGW)
*   **Raylib** (Automatically linked from local `lib/` folder if present, or system paths)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/WitcherTro/boats.git
cd boats

# Configure CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build
```

This will generate three executables in the `build/` (or `build/bin/`) directory:
*   `server`
*   `client_cli`
*   `client_gui` (if Raylib is found)

## 🚀 Usage

1.  **Start the Server**:
    runs on port 12345 by default.
    ```bash
    ./build/server
    ```

2.  **Start Clients**:
    Open two new terminals/windows for the players.
    ```bash
    # CLI Mode
    ./build/client_cli 127.0.0.1 12345
    
    # GUI Mode
    ./build/client_gui 127.0.0.1 12345
    ```

3.  **Play**:
    *   Enter your name.
    *   Place your ships.
    *   Take turns firing at the enemy grid!

## 📂 Project Structure

*   `src/server`: Multi-threaded server logic using POSIX threads.
*   `src/client/cli`: Terminal user interface implementation.
*   `src/client/gui`: Raylib-based graphical rendering.
*   `src/common`: Shared protocol, networking utilites, and game constants.
*   `lib/`: Contains static libraries for cross-platform support.

## ⚙️ CI/CD

This project uses **GitHub Actions** to automatically build and verify code on push:
*   **Windows**: MinGW build.
*   **Ubuntu**: GCC build with system dependencies.
*   **macOS**: Intel (x86_64) and Apple Silicon (ARM64) builds.

---
*Created by [WitcherTro](https://github.com/WitcherTro)*
