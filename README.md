# N-Type

A C++20/SFML side-scrolling multiplayer prototype. Up to four animated tooth
characters share one UDP server. The server controls positions; clients send keys
and display snapshots. Scenery scrolls automatically at a constant speed,
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
./r-type_client 

./r-type_client -n Player -h 127.0.0.1 -p 4242
```

The client opens the illustrated `back_play.png` title screen with a red **PLAY**
button available immediately. The menu soundtrack starts on this screen and
continues through the launcher. Click PLAY or press Enter to open connection.

The player name is empty by default. Host and port start at `127.0.0.1:4242`;
command-line options override these defaults. Enter a name and select
**Se connecter** to reach the main menu:

- **Join lobby** opens a separate list with selection and a **Rejoindre** button.
  An empty list displays a message. The mouse wheel scrolls longer lists.
- **Create lobby** opens the existing name and capacity form (2–4 players), now
  styled with FreeGUI. Creating a lobby adds it to the list.
- **Play solo** checks the selected server and starts the game directly.
- **Meilleur score** reserves a dedicated panel; `--` is a placeholder, with no
  score calculation or saving yet.

Lobby creation, selection and player counts are currently local UI state. They
are not discovered or synchronized between clients, and the list is reset when
the launcher is recreated after a game. Multiplayer waiting enables **Jouer**
only when the displayed capacity is full; live room membership is not implemented.
Use Play solo to enter the shared server now. There is no map or ambience selector.

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

The same native window is reused when starting, restarting and returning to the
menu. Its size and position are retained; **F11** switches fullscreen and restores
the previous windowed size and position, even across screen transitions. Gameplay
always shows the full 1280×720 play area with bars when needed, without stretching.
Escape returns to the previous launcher screen; on the title screen it quits.

On another computer, replace `127.0.0.1` with the **server computer's IPv4 address**.
Windows uses `r-type_server.exe` and `r-type_client.exe` with the same arguments.
Linux and Windows clients use the same binary protocol and can join the same server.
Allow inbound **UDP 4242** on the server firewall. Internet hosting also requires
appropriate router/hosting configuration; a LAN address is only reachable on that LAN.

- Move your tooth: arrows, WASD or ZQSD. Its wings animate continuously, including
  while stationary, using frames 0 → 1 → 2 → 1 at 8 steps per second. Animation
  changes only the sprite, not server positions or collision bounds. Space sends
  "fire" (not used by the server yet).
- Pause menu: Escape or the menu icon. It can resume, restart, return to the lobby,
  or toggle music and click sounds. The pause controls also use FreeGUI assets.
  This menu stops local movement input; the multiplayer server keeps running.
  Scenery and wing animation continue while it is open. Close the window to quit.
- Music loops automatically: `audio/music/lobby.mp3` in the launcher, then
  `level_01_ice.mp3`, `level_02_candy.mp3`, or `level_03_dentist_lab.mp3`
  according to the current automatic level. The Son ON/OFF preference is
  retained when restarting or returning to the lobby.
- Lobby, gameplay and restart reuse the same SFML window. The active music is
  stopped before the next screen starts, so tracks never overlap.
- The window title identifies your player and connection status.
- A fifth player waits with a "Server full" status and retries automatically.
- Before connecting, or after a connection timeout, no player sprites are displayed.

## Files

| File | Responsibility |
|------|----------------|
| [engine/](engine/) | Shared ECS: World, components, Movement and Draw systems |
| [ClientApp.cpp](client/src/ClientApp.cpp) | Window and keyboard; mirrors snapshots into an engine World and draws it |
| [Launcher.cpp](client/src/Launcher.cpp) | Title, connection, main menu, lobby forms and settings |
| [ControlsMenu.cpp](client/src/ControlsMenu.cpp) | Shared visual keyboard/mouse/gamepad settings; no input remapping |
| [GuiSkin.hpp](client/src/GuiSkin.hpp) | Shared FreeGUI drawing and smooth text helpers |
| [WindowState.hpp](client/src/WindowState.hpp) | Shared window bounds and fullscreen restoration |
| [ClientNetwork.cpp](client/src/ClientNetwork.cpp) | Send keys, receive snapshots and reconnect |
| [main.cpp](client/src/main.cpp) | Start the client and parse options |
| [server/src/main.cpp](server/src/main.cpp) | Start the server and parse options |
| [Server.cpp](server/src/Server.cpp) | Accept up to four players, move them with the engine and send snapshots (game thread) |
| [NetworkThread.cpp](server/src/NetworkThread.cpp) | UDP socket, receives and sends on its own thread |
| [ThreadSafeQueue.hpp](server/src/ThreadSafeQueue.hpp) | Hands received packets from the network thread to the game thread |
| [Protocol.cpp](common/protocol/Protocol.cpp) | Binary encoding and decoding of every message ([PROTOCOL_v2.md](docs/PROTOCOL_v2.md)) |
| [GameConfig.hpp](shared/GameConfig.hpp) | Settings both sides agree on: port, world size, speed, player count |
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
the entity's top-left corner. X stays within 0…1240 and Y within 0…680 in the
1280×720 viewport, with 40×40 entities moving at 300 pixels/s. Players can traverse
the entire viewport and are stopped at all four edges. Invalid packets are ignored.

## Verification

After building, run the protocol unit test and the black-box server checks
(Python 3, standard library only):

```sh
ctest --test-dir build/client-linux-release --output-on-failure
python3 tests/network_smoke.py ./r-type_server
```

They check connection, movement across the viewport and all four boundaries, up to four
players, independent movement, malformed packets, refusal of a fifth player,
repeated connects, disconnects, id reuse and crash timeouts.
For visual checks:

1. Start without `-n`: PLAY must be visible immediately, with menu music. The
   connection name must be empty and host/port prefilled.
2. Check the FreeGUI connection, main menu, lobby list, creation and settings;
   the best-score card must show `--` and the main menu must have no title ribbon.
3. Enter Play solo. Leave the tooth stationary and check that its wings keep
   flapping. Move to all four edges: the entire play area must remain visible,
   and background speed must stay constant in every direction.
4. Open pause with Escape or the icon. Check Continue, Restart, Lobby, music and
   click-sound controls, including their labels and FreeGUI artwork. Open Controles
   from both settings and pause; switch tabs, select a row, return with Escape
   and Retour, and verify that gameplay bindings remain unchanged.
5. Resize, restart, return to the menu and launch again. Window dimensions must
   stay the same. Toggle F11 in one screen and leave fullscreen in another:
   the previous windowed size and position must return.
6. Start a second client: both teeth must animate, and the second disappears when
   its client disconnects.

Code follows the simple conventions in [the project guide](docs/PROJECT_GUIDE.md#7-conventions).
The root `.clang-format` records indentation, braces and spacing for future edits.
