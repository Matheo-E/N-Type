# N-Type

A networked multiplayer remake of **R-Type**, built on a small reusable game engine.

Up to **four players** team up against the Bydos. The server runs the whole game; clients only send their inputs and display what the server tells them.

| Binary          | Role                                              |
|-----------------|---------------------------------------------------|
| `r-type_server` | Authoritative, multithreaded UDP game server      |
| `r-type_client` | Graphical client (SFML)                           |

---

## Quick start

```sh
# 1. Build (first run takes a while, see below)
./build.sh            # Linux
build.bat             # Windows

# 2. Start a server
./r-type_server -p 4242

# 3. Connect a client (run once per player)
./r-type_client -h 127.0.0.1 -p 4242
```

The build script puts both binaries at the root of the repository.

---

## Requirements

### Linux

- git
- CMake 3.25+
- Ninja
- GCC 14+ **or** Clang 18+
- X11, OpenGL and udev development headers (needed by SFML)

The build script checks for the headers and prints the install command for Debian/Ubuntu or Fedora if any are missing.

### Windows

- git
- **Visual Studio 2022+** with the **Desktop development with C++** workload.
  You don't need the full IDE: the free **Build Tools for Visual Studio** are enough, and you can keep using any editor (VS Code, VSCodium...).

  ```powershell
  winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --wait"
  ```

---

## Building

```sh
./build.sh [options]    # Linux
build.bat  [options]    # Windows
```

The script does everything:

1. Fetches vcpkg on the first run, or uses your own if `VCPKG_ROOT` is set.
2. Builds the dependencies (SFML, GoogleTest...).
3. Builds the project.
4. Copies `r-type_server` and `r-type_client` to the repository root.

**The first build is slow** because the dependencies are compiled from source. Later builds only recompile what changed.

### Options

| Option          | Effect                                                   |
|-----------------|----------------------------------------------------------|
| `--debug`       | Debug build instead of release                           |
| `--clean`       | Delete previous build files and start fresh              |
| `--server-only` | Linux only: skip the client and SFML entirely            |
| `--help`        | List all options                                         |

### Building by hand

If you prefer driving CMake yourself, set `VCPKG_ROOT` to a vcpkg checkout, then:

```sh
cmake --preset linux-release          # or windows-release
cmake --build --preset linux-release
ctest --preset linux-release          # run the tests
```

On Windows, run these from a *Developer PowerShell / Command Prompt for VS*. The binaries end up in `build/<preset>/bin/`.

---

## Playing

```sh
./r-type_server -p 4242
./r-type_client -h <server-ip> -p 4242
```

Use `127.0.0.1` to play on the same machine. Both binaries list their options with `--help`.

| Action | Keys                         |
|--------|------------------------------|
| Move   | Arrow keys, ZQSD or WASD     |
| Fire   | Space                        |
| Quit   | Esc                          |

---

## Troubleshooting

**`Visual Studio with the C++ tools was not found`**
Install the Build Tools with the C++ workload (see [Requirements](#windows)), then restart your terminal **and** your editor.

**`Could not resolve host: github.com` during `vcpkg install`**
vcpkg downloads its package data from GitHub during the first build. Check your connection and run the script again; it resumes where it stopped. Some school or public networks block or throttle GitHub; a phone hotspot is a quick workaround.

**`CMake was unable to find a build program corresponding to "Ninja"`**
This message often appears only as a side effect of a failed vcpkg step: fix that error first. If it persists, install Ninja (`winget install Ninja-build.Ninja` on Windows), restart your editor so it picks up the new `PATH`, and rebuild with `--clean`.

---

## Project layout

| Path            | Content |
|-----------------|---------|
| `engine/`       | `ntype-engine`: the game-agnostic engine (ECS, events, time, scenes, networking) and its SFML backend |
| `rtype_common/` | R-Type rules shared by client and server: components, systems, protocol |
| `server/`       | `r-type_server` |
| `client/`       | `r-type_client` |
| `assets/`       | Sprites, sounds, font, and `data/game.json` (enemies, waves, weapons) |
| `tests/`        | Unit tests (GoogleTest) |
| `tools/`        | Asset generation scripts |
| `docs/`         | Architecture, protocol and contribution docs |

Layers are enforced by the CMake target graph: each library only sees the headers of the libraries it links to. Including a `client/` or `server/` header from `engine/` or `rtype_common/` is a compile error.

---

## Documentation

| Topic          | Document |
|----------------|----------|
| Architecture   | [Architecture plan](docs/ARCHITECTURE_PLAN.md), [class diagrams](docs/CLASS_UML.md) |
| Networking     | [Network protocol](docs/PROTOCOL.md) |
| Contributing   | [Coding rules](docs/ARCHITECTURE.md), [cross-platform rules](docs/COMPATIBILITY.md), [Git workflow](GIT_WORKFLOW.md) |

---

## Credits

- **Sprites:** original R-Type sprite sheets provided with the subject.
- **Font:** *Press Start 2P* by CodeMan38, SIL Open Font License (`assets/fonts/OFL.txt`).
- **Sound effects:** generated by `tools/generate_sounds.py`.
