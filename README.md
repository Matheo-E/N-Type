# N-Type — Networked squares with lobbies

Each player starts the client, chooses a name, then creates a lobby or joins one.
One UDP server hosts up to 32 lobbies; **each lobby runs its own game** for up to
four players. The creator leads the lobby: they rename it and start the game (the map
follows the level: level 1 plays map 1, and so on). The server computes all movement; clients
send keys and display server positions.

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
(network); there is no audio or external asset (menus use a system font and plain shapes until the menu art arrives). On Windows, keep the
copied DLLs next to the executables. In PowerShell, prefix commands with `.\`.

## Run

Start one server:

```sh
./r-type_server -p 4242
```

Start one client per player (in separate terminals or on different computers):

```sh
./r-type_client -n Ann -h 127.0.0.1 -p 4242
```

The options only prefill the connection page, where the name, host and port can
be changed before pressing **Continue**. The window can be resized or maximized
(press **F11** for fullscreen); the view keeps its proportions.

On another computer, replace `127.0.0.1` with the **server computer's IPv4 address**.
Windows uses `r-type_server.exe` and `r-type_client.exe` with the same arguments.
Linux and Windows clients use the same binary protocol and can join the same server.
Allow inbound **UDP 4242** on the server firewall. Internet hosting also requires
appropriate router/hosting configuration; a LAN address is only reachable on that LAN.

1. **Connect** — name (1–16 characters), host and port, then Continue.
2. **Lobbies** — the list shows every lobby (name, room code, players `n/4`,
   Waiting or Playing). Double-click one or use "Join selected", type a 4-character
   **room code** and press Join, or press "Create lobby".
3. **Create** — lobby name (prefilled "<name>'s lobby"); you become the leader.
4. **Waiting room** — shows the room code and the members. The leader can rename
   the lobby and press Start; anyone can Leave.
5. **Game** — move your square with arrows, WASD or ZQSD. Space sends "fire" (not
   used by the server yet).

- Joining a lobby that is playing makes you a **spectator**: you watch and become a
  player when a new level starts or when the lobby returns to the waiting room.
- If the leader leaves, the member present the longest becomes the leader.
- Esc goes back one page (in game or in the waiting room: leave the lobby; on the
  lobby list: disconnect; on the connection page: quit). F11 toggles fullscreen.
- A lost connection brings you back to the connection page, which reconnects automatically.

Levels and game over do not exist yet. Until they do, type these commands in the
server's terminal (the code is the lobby's room code):

- `level <code>` — a new level starts: spectators become players.
- `gameover <code>` — everyone is game over: the lobby returns to the waiting room.

## Files

| File | Responsibility |
|------|----------------|
| [engine/](engine/) | Minimal ECS (World, components, Movement and Draw systems), maintained on the `Engine` branch |
| [ClientApp.cpp](client/src/ClientApp.cpp) | The window: shows the screen matching the session's phase, F11 fullscreen, keyboard |
| [ClientNetwork.cpp](client/src/ClientNetwork.cpp) | The session: connect, lobby list, lobby requests re-sent until confirmed, snapshots |
| [screens/](client/src/screens/) | One class per page: Connect, Lobby list, Create, Waiting room, Game |
| [ui/](client/src/ui/) | `Theme` (fonts, colours, future menu textures) and widgets (Button, TextField, ListView) |
| [main.cpp](client/src/main.cpp) | Start the client and parse options |
| [server/src/main.cpp](server/src/main.cpp) | Start the server and parse options |
| [Server.cpp](server/src/Server.cpp) | Clients, lobby requests, one game per lobby, snapshots and lobby states (game thread) |
| [Lobby.cpp](server/src/Lobby.cpp) | One lobby's rules: slots, leader, spectators, start, level, game over |
| [Lobbies.cpp](server/src/Lobbies.cpp) | Every lobby by room code; creates the codes |
| [Game.cpp](server/src/Game.cpp) | One running game: ships, inputs, movement, snapshot |
| [Console.cpp](server/src/Console.cpp) | Reads the debug commands typed in the server's terminal |
| [NetworkThread.cpp](server/src/NetworkThread.cpp) | UDP socket, receives and sends on its own thread |
| [ThreadSafeQueue.hpp](server/src/ThreadSafeQueue.hpp) | Hands received packets from the network thread to the game thread |
| [Protocol.cpp](common/protocol/Protocol.cpp) | Binary encoding and decoding of every message ([PROTOCOL_v2.md](docs/PROTOCOL_v2.md)) |
| [GameConfig.hpp](shared/GameConfig.hpp) | Settings both sides agree on: port, world size, speed, player and client counts |
| [Options.hpp](shared/Options.hpp) | Shared command-line parsing |

The server uses **two threads**: a network thread runs Asio and queues every received
datagram, and the main thread runs the game loop at 60 ticks/s, reads that queue each
tick, steps every playing lobby and sends each lobby's snapshot to its members. Only the network thread touches
the socket. This is a lobby/connection prototype, not a full R-Type. There is no prediction,
interpolation, authentication or reliable-message layer: the server re-sends each
lobby's state 10 times per second, and the client repeats each lobby request every
250 ms until that state shows its effect (see the protocol document). ConnectRequest
is retried every 500 ms until answered. Disconnect is sent three times; a 5-second
timeout handles lost disconnects and crashes.

## Binary protocol

The full specification is [docs/PROTOCOL_v2.md](docs/PROTOCOL_v2.md): one type byte,
then the payload, little-endian. This demo uses these messages:

| Type | Direction | Use here |
|------|-----------|----------|
| 1 ConnectRequest | Client → Server | Connect with your name |
| 2 ConnectAccept | Server → Client | You are connected |
| 3 ConnectRefuse | Server → Client | Reason 1 (ServerFull) when 128 clients are connected |
| 4 Disconnect | Client → Server | Leave the server |
| 5 Input | Client → Server | Keys, sent every frame; it is also the heartbeat |
| 9 WorldSnapshot | Server → Client | Every player of your lobby, as an entity of kind 1 whose id is its slot (1–4) |
| 11 ClientLeft | Server → Client | A player left your playing lobby |
| 13 LobbyListRequest / 14 LobbyList | Both | The list of lobbies |
| 15 LobbyCreate / 16 LobbyJoin / 18 LobbyLeave | Client → Server | Create, join by room code, leave |
| 17 LobbyRefuse | Server → Client | NotFound, Full or TooManyLobbies |
| 19 LobbyState | Server → Client | Your lobby: code, name, state, members, leader, your slot |
| 20 LobbyRename / 21 LobbyStart | Client → Server | Leader actions |

Messages 6–8, 10 and 12 (entity spawn/update/destroy, player events, game state) are
not used yet. Keys: up=1, down=2, left=4, right=8, shoot=16. Slots 1–4 are blue,
orange, green and pink; a freed slot is reused by the next member. Positions refer to
the square's top-left corner, within x=0…1240, y=0…680 in a 1280×720 window, with
40×40 squares moving at 300 pixels/s. Invalid packets are ignored.

## Verification

After building, run the unit tests and the black-box server checks
(Python 3, standard library only):

```sh
ctest --test-dir build/client-linux-release --output-on-failure
python3 tests/network_smoke.py ./r-type_server
```

`ctest` runs the protocol, game, lobby, client-session and widget tests. The smoke
test checks the lobby list, joining by code, leader-only actions, spectators, the
`level`/`gameover` commands, leaving, two lobbies playing at once, a full server and
crash timeouts. For the windows, start three clients and go through create, join from
the list, join by code, start, spectate and Esc back to the list.
