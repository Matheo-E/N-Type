# N-Type — Networked squares

A small demonstration: one square per connected player, up to four players on
one UDP server. One player alone sees one square. Each new player's square appears
on every client when they connect. Each client controls only its own square.
The server computes all movement; clients send keys and display server positions.

## Build

Linux: git, CMake 3.25+, Ninja, a C++20 compiler and SFML's X11/OpenGL/GLU/udev headers.
Windows: git and Visual Studio 2022+ with the Desktop development with C++ workload.

```sh
./build.sh       # Linux
```

```bat
build.bat       rem Windows Command Prompt
```

The scripts build and copy both executables to the repository root. They support
`--debug`, `--clean` and `--help`. vcpkg provides SFML 2.6 (window/graphics) and Asio
(network); there is no audio, external asset or menu. On Windows, keep the
copied DLLs next to the executables. In PowerShell, prefix commands with `.\`.

## Run

Start one server:

```sh
./r-type_server -p 4242
```

Start one client per player (in separate terminals or on different computers):

```sh
./r-type_client 

./r-type_client -n Player -h 127.0.0.1 -p 4242
```

The client first opens a small SFML connection window. Enter a player name, host
and port, then select **Connect** to open the game. Command-line values are used
as the form defaults. The window can be freely resized or maximized; press **F11**
to enter or leave fullscreen mode. The game window has the same controls. Both
windows preserve their aspect ratio so text and graphics are never stretched.

On another computer, replace `127.0.0.1` with the **server computer's IPv4 address**.
Windows uses `r-type_server.exe` and `r-type_client.exe` with the same arguments.
Linux and Windows clients use the same binary protocol and can join the same server.
Allow inbound **UDP 4242** on the server firewall. Internet hosting also requires
appropriate router/hosting configuration; a LAN address is only reachable on that LAN.

- Move your square: arrows, WASD or ZQSD. Space sends "fire" (not used by the server yet).
- Quit: Escape or the window close button.
- The window title identifies your player and connection status.
- A fifth player waits with a "Server full" status and retries automatically.
- Before connecting, or after a connection timeout, no squares are displayed.

## Files

| File | Responsibility |
|------|----------------|
| [engine/](engine/) | Minimal ECS (World, components, Movement and Draw systems), maintained on the `Engine` branch |
| [ClientApp.cpp](client/src/ClientApp.cpp) | Window and keyboard; mirrors snapshots into an engine World and draws it |
| [ClientNetwork.cpp](client/src/ClientNetwork.cpp) | Send keys, receive snapshots and reconnect |
| [main.cpp](client/src/main.cpp) | Start the client and parse options |
| [server/src/main.cpp](server/src/main.cpp) | Start the server and parse options |
| [Server.cpp](server/src/Server.cpp) | Accept up to four players, move them with the engine and send snapshots (game thread) |
| [NetworkThread.cpp](server/src/NetworkThread.cpp) | UDP socket, receives and sends on its own thread |
| [ThreadSafeQueue.hpp](server/src/ThreadSafeQueue.hpp) | Hands received packets from the network thread to the game thread |
| [Protocol.cpp](common/protocol/Protocol.cpp) | Binary encoding and decoding of every message ([PROTOCOL_v2.md](docs/PROTOCOL_v2.md)) |
| [GameConfig.hpp](shared/GameConfig.hpp) | Settings both sides agree on: port, world size, speed, player count |
| [Options.hpp](shared/Options.hpp) | Shared command-line parsing |

The server uses **two threads**: a network thread runs Asio and queues every received
datagram, and the main thread runs the game loop at 60 ticks/s, reads that queue each
tick and sends a snapshot of every player each tick. Only the network thread touches
the socket. This is a connection/window prototype, not a full R-Type. There is no
prediction, interpolation, authentication or reliable-message layer. ConnectRequest is
retried every 500 ms until the server answers ConnectAccept. Snapshots are sent every
tick, so a lost one is replaced by the next. Disconnect is sent three times; a 5-second
timeout handles lost disconnects and crashes.

## Binary protocol

The full specification is [docs/PROTOCOL_v2.md](docs/PROTOCOL_v2.md): one type byte,
then the payload, little-endian. This demo uses these messages:

| Type | Direction | Use here |
|------|-----------|----------|
| 1 ConnectRequest | Client → Server | Join the game |
| 2 ConnectAccept | Server → Client | Your player id (1–4) |
| 3 ConnectRefuse | Server → Client | Reason 1 (ServerFull) when four players are in |
| 4 Disconnect | Client → Server | Leave the game |
| 5 Input | Client → Server | Keys, sent every frame; it is also the heartbeat |
| 9 WorldSnapshot | Server → Client | Every player, as an entity of kind 1 whose id is the player id |
| 11 ClientLeft | Server → Client | A player left or timed out |

Messages 6–8, 10 and 12 (entity spawn/update/destroy, player events, game state) are
not used yet. Keys: up=1, down=2, left=4, right=8, shoot=16. Players 1–4 are blue,
orange, green and pink; a freed id is reused by the next player. Positions refer to
the square's top-left corner, within x=0…1240, y=0…680 in a 1280×720 window, with
40×40 squares moving at 300 pixels/s. Invalid packets are ignored.

## Verification

After building, run the protocol unit test and the black-box server checks
(Python 3, standard library only):

```sh
ctest --test-dir build/client-linux-release --output-on-failure
python3 tests/network_smoke.py ./r-type_server
```

They check connection, up to four players, independent movement, malformed packets,
refusal of a fifth player, repeated connects, disconnects, id reuse and crash timeouts.
For the window, start two real clients and check that the second square appears
and disappears as the other client connects and quits.
