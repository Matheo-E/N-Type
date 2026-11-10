# User Guide

This guide is for players and server hosts. No programming knowledge is required.

## Install and build

First download the source code:

```sh
git clone https://github.com/Matheo-E/N-Type.git
cd N-Type
```

Alternatively, use **Code → Download ZIP** on GitHub, extract the archive, and
open a terminal in the extracted directory. Then install the requirements and
build the project by following the [main README](../README.md#quick-start).

The build creates:

- `r-type_server`: hosts connections, lobbies, and games.
- `r-type_client`: displays the menus/game and sends player input.

The programs may run on the same or different computers. Linux and Windows builds
use the same protocol and can play together.

## Start a local game

Open two terminals in the repository root:

```sh
./r-type_server
```

```sh
./r-type_client
```

The default address is `127.0.0.1:4242`. Enter a name of 1–16 printable ASCII
characters, select **Se connecter**, and choose solo play or create a lobby.

```text
r-type_client [-n name] [-h host] [-p port]
r-type_server [-p port] [--no-enemies]
```

`--no-enemies` creates ship-only games and is intended for network tests.

## Lobbies

- **Join lobby** lists server lobbies or accepts a four-character room code. The
  list refreshes once per second.
- **Create lobby** creates a named room and makes you its leader.
- **Play solo** creates and immediately starts a run on the same server.

A lobby holds four members. The leader can rename and start it. If the leader
leaves, leadership passes to the longest-present member. Someone joining an active
game spectates until the next level. Freed player slots are reused.

## Gameplay

Move with arrow keys, WASD, or ZQSD and shoot with Space. Each player starts with
3 HP. After death, the player watches until the next level. Defeating a boss
advances the lobby and revives missing players. The run ends when no player remains
or the final boss dies.

Enemy kills award their configured score to the player whose projectile caused
the kill. Results are sorted by score and include the team total. Scores survive
until the next run starts but are not saved between sessions.

Escape opens the pause menu. It only stops your local input; the shared game keeps
running. F11 toggles fullscreen. Mouse/controller bindings and remapping shown in
Controls are proposed interfaces, not active gameplay controls.

## Host over a network

1. Start the server on the host computer.
2. Find its LAN IPv4 address.
3. Allow inbound UDP traffic on the chosen port (4242 by default).
4. Start clients with `-h <server-ip> -p <port>`.

A private address such as `192.168.x.x` works only on that LAN. Internet hosting
usually requires UDP port forwarding or a public host. Traffic is neither
authenticated nor encrypted; do not expose a test server where that is unacceptable.

## Server console

```text
level <room-code>
gameover <room-code>
score <room-code> <slot 1-4> <points>
```

These development commands advance a level, force a defeat, or add score. Room
codes appear in the waiting room and pause menu.

## Troubleshooting

### Cannot connect

- Confirm the server is running and both programs use the same UDP port.
- Use `127.0.0.1` only on the server computer itself.
- Check firewall and, for internet use, UDP forwarding.
- Names accept only 1–16 printable ASCII characters (no accents).

### Missing assets or audio

Run copied binaries from the repository root. CMake embeds the source asset path;
reconfigure after moving the repository and verify the `assets/` tree is complete.

### Bars around the game

This is expected. The logical 1280×720 area preserves its aspect ratio.

### A displayed control does nothing

Only keyboard movement, Space, Escape, and F11 are active today.

### Disconnection

Input is also a heartbeat. A client silent for more than five seconds is removed.
Connection and lobby operations retry automatically, but an interrupted session
may still require returning to the connection screen.
