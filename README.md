# N-Type

N-Type is a cooperative, side-scrolling shooter written in C++20. Up to four
players control animated teeth through three themed levels, fight server-driven
enemy waves, collect scores, and defeat a boss at the end of each level.

The project includes a graphical SFML client, an authoritative UDP server, a
small reusable ECS engine, a binary network protocol, JSON-driven game content,
and automated tests.

## Highlights

- Online lobbies with room codes, leader transfer, spectators, and solo play
- One server hosting up to 32 independent lobbies and 128 connected clients
- Server-authoritative movement, combat, waves, scores, and level progression
- Three levels with parallax backgrounds, music, enemies, and bosses
- Data-driven enemies, attacks, flight patterns, items, levels, and textures
- Linux and Windows builds through CMake and vcpkg

## Quick start

### Requirements

- Git, CMake 3.25+, Ninja, a C++20 compiler, and vcpkg
- Linux: pkg-config, Autotools, and SFML's X11/OpenGL/GLU/udev system headers
- Windows: Visual Studio 2022+ with **Desktop development with C++**

On Debian, Ubuntu, or Pop!_OS:

```sh
sudo apt install build-essential git cmake ninja-build pkg-config \
    autoconf autoconf-archive automake libtool \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libudev-dev
```

### Download the project

Clone the repository, then enter its directory:

```sh
git clone https://github.com/Matheo-E/N-Type.git
cd N-Type
```

If you do not use Git, open the repository on GitHub, select **Code → Download
ZIP**, extract the archive, and open a terminal inside the extracted `N-Type`
directory. All commands below must be run from that directory.

### Build

```sh
./build.sh
```

```bat
build.bat
```

Both scripts place `r-type_server` and `r-type_client` in the repository root.
They support `--debug`, `--clean`, and `--help`. Windows also receives the runtime
DLLs required beside the executables.

### Run

```sh
./r-type_server -p 4242
```

In another terminal, start one client per player:

```sh
./r-type_client -n Ann -h 127.0.0.1 -p 4242
```

On Windows, use the corresponding `.exe` files. For LAN play, replace `127.0.0.1`
with the server computer's IPv4 address and allow inbound UDP port 4242.

## How to play

1. Enter a name and connect.
2. Create a lobby, join one from the list or by room code, or choose solo play.
3. The lobby leader starts the game. Late joiners spectate until the next level.
4. Survive the waves and defeat each boss. Dead players return at the next level.

| Action | Controls |
| --- | --- |
| Move | Arrow keys, WASD, or ZQSD |
| Shoot | Space |
| Pause / back | Escape |
| Fullscreen | F11 |

The Controls menu is currently a visual preview; input remapping and mouse/gamepad
gameplay are not implemented. See the [User Guide](docs/USER_GUIDE.md) for menus,
hosting, settings, and troubleshooting.

## Documentation

| Document | Purpose |
| --- | --- |
| [User Guide](docs/USER_GUIDE.md) | Install, run, host, play, and troubleshoot |
| [Architecture](docs/ARCHITECTURE.md) | Runtime design, ECS, client/server flow, repository map |
| [Content Guide](docs/CONTENT_GUIDE.md) | Add enemies, items, attacks, levels, and assets |
| [Protocol v2](docs/PROTOCOL_v2.md) | UDP wire format, messages, reliability, validation |
| [API Documentation](docs/DOXYGEN.md) | Generate and view Doxygen on Linux or Windows |
| [Contributing](CONTRIBUTING.md) | Workflow, tests, conventions, pull requests |
| [Enemy Catalogue](docs/ENEMIES.md) | Current enemies and bosses |
| [Draw Layers](docs/DRAW_LAYERS.md) | Rendering order and z-index conventions |
| [Asset Layout](assets/README.md) | Runtime asset directories and naming |

## Project structure

```text
client/             SFML launcher, rendering, audio, and network session
server/             authoritative lobbies, simulation, console, UDP transport
common/protocol/    shared binary encoder and decoder
engine/             ECS, factories, systems, combat, and JSON loaders
shared/             shared constants and command-line parsing
assets/             JSON data, sprites, backgrounds, music, and effects
tests/              unit, integration, and end-to-end tests
docs/               user, architecture, content, and protocol documentation
```

## Tests

After building:

```sh
ctest --test-dir build/client-linux-release --output-on-failure
python3 tests/network_smoke.py ./r-type_server
```

Use `client-linux-debug` after `./build.sh --debug`. See
[Contributing](CONTRIBUTING.md) for target-specific validation.

## Current limitations

N-Type is an educational prototype, not a production network service. It has no
authentication, encryption, matchmaking, persistent accounts, saved high scores,
client prediction, or snapshot interpolation. UDP events are not guaranteed to
arrive, although connection/lobby requests are retried and full world state is
continuously resent.

## License

No license file is currently included. Unless the maintainers add one, the source
and bundled assets should not be assumed to permit redistribution or reuse.
