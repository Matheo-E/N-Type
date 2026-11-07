# N-Type

A C++20/SFML side-scrolling multiplayer prototype with lobbies. Each player
starts the client, chooses a name, then creates a lobby or joins one. One UDP
server hosts up to 32 lobbies; **each lobby runs its own game** for up to four
animated tooth characters. The server controls positions; clients send keys and
display snapshots. Scenery scrolls automatically at a constant speed,
independently of player movement.

## Build

Linux: git, CMake 3.25+, Ninja, a C++20 compiler, pkg-config, Autotools
(including autoconf-archive for ALSA), and SFML's X11/OpenGL/GLU/udev headers.
Windows: git and Visual Studio 2022+ with the Desktop development with C++ workload.

Install the Linux prerequisites on Debian/Ubuntu/Pop!_OS before building:

```sh
sudo apt install build-essential git cmake ninja-build pkg-config \
    autoconf autoconf-archive automake libtool \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libudev-dev
```

```sh
./build.sh       # Linux
```

```bat
build.bat       rem Windows Command Prompt
```

The scripts build and copy both executables to the repository root. They support
`--debug`, `--clean` and `--help`. vcpkg provides SFML 2.6 (window/graphics) and Asio
(network); SFML audio loads the music and sound effects stored under `assets/`.
On Windows, keep the copied DLLs next to the executables. In PowerShell, prefix
commands with `.\`.

## Run

Start one server:

```sh
./r-type_server -p 4242
```

Start one client per player (in separate terminals or on different computers):

```sh
./r-type_client -n Ann -h 127.0.0.1 -p 4242
```

The client opens the illustrated `back_play.png` title screen with a red **PLAY**
button available immediately. The menu soundtrack starts on this screen and
continues through the launcher. Click PLAY or press Enter to open connection.

The player name is empty by default. Host and port start at `127.0.0.1:4242`;
command-line options override these defaults. Enter a name (1–16 characters,
no accents) and select **Se connecter**: the main menu opens once the server
accepts the connection.

1. **Join lobby** — the list shows every lobby on the server (name, room code,
   Waiting or Playing, players `n / 4`) and refreshes every second. Select one and
   press **Rejoindre**, or type a 4-character **room code** and press **OK**.
   The mouse wheel scrolls longer lists.
2. **Create lobby** — lobby name (prefilled "<name>'s lobby"); you become the leader.
3. **Play solo** — creates a lobby and starts it immediately.
4. **Salle d'attente** — shows the lobby name, room code and members (colour, leader,
   spectators). The leader clicks the name to rename the lobby and presses
   **Lancer la partie**; the back arrow or Escape leaves the lobby.
5. **Game** — every member of the lobby enters the game when the leader starts it.

- Joining a lobby that is playing makes you a **spectator**: you watch and become a
  player when a new level starts or when the lobby returns to the waiting room.
- If the leader leaves, the member present the longest becomes the leader.
- When the lobby returns to waiting (game over), everyone goes back to its waiting room.
- A lost connection is retried automatically; the status shows at the bottom of the menus.
- **Meilleur score** reserves a dedicated panel; `--` is a placeholder, with no
  score calculation or saving yet.

The menu uses `back_lobby.png` when present, otherwise the supplied
`back_menu.png`. FreeGUI panels, ribbons, fields, buttons and icons style the
connection form, main menu, lobby screens, settings and in-game pause menu.
The supplied game logo is centered above the tooth on the PLAY screen and
shown in a smaller size above the main-menu buttons, without stretching it.
The main menu has no title ribbon. Text is rasterized above its displayed pixel
size and aligned to window pixels to reduce pixelation when resized.

Settings and the pause menu now include **Controles**, with **Clavier**, **Souris**
and **Manette** tabs. FreeGUI buttons show six actions and their displayed bindings;
clicking a binding only highlights its row. Retour or Escape returns to the
previous settings screen. This is a visual interface: it does not capture new
keys, change gameplay input, save bindings, or detect connected devices. Mouse
and gamepad bindings are proposed examples, not active controls.

The same native window is reused by the menus and the game. Its size and position
are retained; **F11** switches fullscreen and restores the previous windowed size
and position, even across screen transitions. Gameplay always shows the full
1280×720 play area with bars when needed, without stretching. Escape returns to
the previous launcher screen; on the title screen it quits.

On another computer, replace `127.0.0.1` with the **server computer's IPv4 address**.
Windows uses `r-type_server.exe` and `r-type_client.exe` with the same arguments.
Linux and Windows clients use the same binary protocol and can join the same server.
Allow inbound **UDP 4242** on the server firewall. Internet hosting also requires
appropriate router/hosting configuration; a LAN address is only reachable on that LAN.

- Move your tooth: arrows, WASD or ZQSD. Its wings animate continuously, including
  while stationary, using frames 0 → 1 → 2 → 1 at 8 steps per second. Animation
  changes only the sprite, not server positions or collision bounds. Space sends
  "fire" (not used by the server yet).
- Pause menu: Escape or the menu icon. It shows the lobby name and room code, can
  resume, leave the lobby (**Lobby**), or toggle music and click sounds. This menu
  stops local movement input; the lobby's game keeps running. Close the window to quit.
- Spectators see a banner and their keys are not sent.
- Music loops automatically: `audio/music/lobby.mp3` in the launcher, then
  `level_01_ice.mp3`, `level_02_candy.mp3`, or `level_03_dentist_lab.mp3`
  according to the current automatic level. The Son ON/OFF preference is
  retained when returning to the menus.
- The active music is stopped before the next screen starts, so tracks never overlap.
- The window title shows your name, the lobby, its room code and your colour.

Levels and game over do not exist yet. Until they do, type these commands in the
server's terminal (the code is the lobby's room code):

- `level <code>` — a new level starts: spectators become players.
- `gameover <code>` — everyone is game over: the lobby returns to the waiting room.

## Files

| File | Responsibility |
|------|----------------|
| [engine/](engine/) | Shared ECS: World, components, Movement and Draw systems |
| [ClientApp.cpp](client/src/ClientApp.cpp) | The game: mirrors the lobby's snapshots into an engine World and draws it; pause menu |
| [Launcher.cpp](client/src/Launcher.cpp) | Title, connection, main menu, lobby list, create, waiting room and settings |
| [ControlsMenu.cpp](client/src/ControlsMenu.cpp) | Shared visual keyboard/mouse/gamepad settings; no input remapping |
| [GuiSkin.hpp](client/src/GuiSkin.hpp) | Shared FreeGUI drawing and smooth text helpers |
| [WindowState.hpp](client/src/WindowState.hpp) | Shared window bounds and fullscreen restoration |
| [ClientNetwork.cpp](client/src/ClientNetwork.cpp) | The session: connect, lobby list, lobby requests re-sent until confirmed, snapshots |
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
| [assets/](assets/) | Organized backgrounds, sprites, music and sound effects |

## Asset organization

All runtime resources live under a single `assets/` root. Backgrounds are kept
by level and ambience under `backgrounds/`; player, enemy and effect sprites are
under `sprites/`; music and sound effects are under `audio/`. Interface controls
use the original `FreeGUI/png/Button.png` and `Window.png` sprite sheets,
with labels rendered by SFML. See [assets/README.md](assets/README.md) for the
complete layout and naming rules.

## Parallax backgrounds

The single adventure progresses through `level_01_ice`, `level_02_candy`, then
`level_03_dentist_lab`. Level 3 remains active until a real boss-driven transition
is implemented. Independently, the ambience alternates between `day` and `night`.
The current short test timings are defined by `kTemporaryLevelDurationSeconds`
and `kDayNightPeriodSeconds`. Every ambience is assembled in this order:
`layer_01_far.png`, `layer_02_middle.png`, `layer_03_near.png`, then
`layer_04_ground.png`.
The layers repeat horizontally with a continuous, time-based scroll independent
of player movement. Each layer uses a constant speed factor, so the scenery keeps
moving at the same speed while the player moves anywhere in the fixed viewport. Server positions and collision bounds
remain authoritative and are not affected by this visual movement.
Day/night changes use a smooth six-second crossfade. The incoming four-layer
scene is preloaded and blended directly into the window.

Background configuration is centralized in
`client/src/BackgroundConfig.hpp`. To tune the scrolling, edit
`kAutomaticScrollSpeed` (base speed in pixels per second) and
`kParallaxFactors` (relative speed of each layer). Set the automatic speed to
zero to stop the background. Timing is controlled by `kDayNightPeriodSeconds` and
`kTemporaryLevelDurationSeconds`; development builds can use shorter values for
testing. Fade length is controlled by `kDayNightFadeDurationSeconds`. To add a
level, add its folder to `kLevelFolders`, its soundtrack to `kMusicFiles`, and
provide both ambience folders with the four ordered layers.

The source strips repeat as supplied. The renderer remains gap-free at every
distance, but a visible artwork seam may occur if replacement images do not have
matching left and right edges.

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
the entity's top-left corner. X stays within 0…1240 and Y within 0…680 in the
1280×720 viewport, with 40×40 entities moving at 300 pixels/s. Players can traverse
the entire viewport and are stopped at all four edges. Invalid packets are ignored.

## Enemy catalogue

`assets/data/enemies.json` defines twelve enemies across the three levels,
including bosses Theice (200 HP), Candybear (250 HP) and Carry (500 HP).
Their sprites, planned attacks, damage, delays and stronger boss phases are
documented in [docs/ENEMIES.md](docs/ENEMIES.md). The new combat fields are
prepared data: the current engine ignores them, and the network server does not
yet spawn these enemies. This does not activate combat or best-score tracking.

## Verification

After building, run the unit tests and the black-box server checks
(Python 3, standard library only):

```sh
ctest --test-dir build/client-linux-release --output-on-failure
python3 tests/network_smoke.py ./r-type_server
```

`ctest` runs the protocol, game, lobby and client-session tests. The smoke test
checks the lobby list, joining by code, leader-only actions, spectators, the
`level`/`gameover` commands, leaving, two lobbies playing at once, movement to all
four edges, a full server and crash timeouts. For visual checks:

1. Start without `-n`: PLAY must be visible immediately, with menu music. The
   connection name must be empty and host/port prefilled.
2. Check the FreeGUI connection, main menu, lobby list, creation, waiting room and
   settings; the best-score card must show `--` and the main menu must have no title ribbon.
3. Use Play solo. Leave the tooth stationary and check that its wings keep
   flapping. Move to all four edges: the entire play area must remain visible,
   and background speed must stay constant in every direction.
4. Open pause with Escape or the icon. Check Continuer, the lobby name and code, Lobby,
   music and click-sound controls. Open Controles from both settings and pause; switch
   tabs, select a row, return with Escape and Retour, and verify that gameplay bindings
   remain unchanged.
5. Start three clients: create a lobby in one, join it from the list in the second and
   by room code in the third, rename it and start it from the leader. Join a playing
   lobby to check the spectator banner, then type `gameover <code>` in the server:
   every client must return to the waiting room.
6. Resize and toggle F11 across screens: the previous windowed size and position must return.

Code follows the simple conventions in [the project guide](docs/PROJECT_GUIDE.md#7-conventions).
The root `.clang-format` records indentation, braces and spacing for future edits.
