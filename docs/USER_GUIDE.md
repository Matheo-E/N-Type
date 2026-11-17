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
r-type_server [-p port] [--no-enemies] [--invincible]
```

`--no-enemies` creates ship-only games and is intended for network tests.
`--invincible` protects all players from damage for temporary gameplay testing;
restart without this flag to restore normal damage.

## Lobbies

- **Join lobby** lists server lobbies or accepts a four-character room code. The
  list refreshes once per second.
- **Create lobby** creates a named room and makes you its leader.
- **Play solo** creates and immediately starts a run on the same server.

A lobby holds four members. The leader can rename and start it. If the leader
leaves, leadership passes to the longest-present member. Someone joining an active
game spectates until the next level. Freed player slots are reused.

## Gameplay

You are **M. Jean Cive**, a tooth on the way to the dentist who gets attacked.
Cross the ice, candy land and dentist's lab, alone or with up to four players.
Dodge enemy shots and orange warning zones before they turn red, shoot toothpaste,
collect useful items and defeat each boss.

Move with arrow keys, WASD, or ZQSD and shoot with Space. Each player starts with
3 HP. After death, the player watches until the next level. Defeating a boss
advances the lobby and revives missing players. The run ends when no player remains
or the final boss dies.

Enemy kills award their configured score to the player whose projectile caused
the kill. Results are sorted by score and include the team total. Scores survive
until the next run starts. Personal best scores are saved locally per exact player
name and displayed in the main menu and waiting room.

Escape opens the pause menu. It only stops your local input; the shared game keeps
running. F11 toggles fullscreen.

## Controls, accessibility and help

Settings and the pause menu include **Controles**, with **Clavier**, **Souris**
and **Manette** tabs. The selected tab is the active gameplay device. Click an
action, then press a key, mouse button or gamepad button to change it. Escape
cancels capture, and F11 remains reserved for fullscreen. Reusing an assigned
button swaps the two actions. **Par defaut** restores controls without clearing
scores. Changes apply immediately and survive client restarts.

Keyboard defaults retain arrows, WASD/ZQSD, Space to fire and Escape to pause.
Mouse mode moves toward the cursor in the play area, with left click to shoot
and right click to pause. Gamepad mode uses the first connected controller's left
stick or D-pad (25% dead zone), button 1 to shoot and button 8 to pause; buttons
can be reassigned. The configured keyboard pause key also works in these modes.
Close the window or use the menu icon if a controller is disconnected.

**Accessibilite** is available from launcher settings and the in-game pause menu.
Three independent options apply immediately and are saved across restarts:

- **Mouvements reduits** freezes background scrolling and removes camera shake.
- **Sans flashs d'impact** removes enemy/player hit flashes and player blinking.
  Health indicators, damage sprites and danger zones remain visible.
- **Tir automatique** fires continuously without holding a button, with the normal
  server fire rate. It stops during local pause, in menus, when the window loses
  focus, and for spectators.

All three options are OFF by default. Use the mouse, or Tab/Up/Down followed by
Enter/Space in the accessibility panel; Escape returns. Resetting controls keeps
these preferences and personal records. Save failures are displayed in the panel.
These are client-side options; they do not change collisions or game rules.

**Comment jouer** is available on the title screen, in settings and in pause.
You are **M. Jean Cive**, a tooth on the way to the dentist who gets attacked.
Cross the ice, candy land and dentist's lab, dodge shots and danger zones, fire
toothpaste, collect items and defeat each boss, alone or with up to four players.
The help panel displays the current device's bindings. Controls can be changed
under **Controles**. Local pause blocks your inputs, but the multiplayer game
continues running.

Scores are local to this computer and indexed by the exact player name; controls
are shared by all local names. Scores, controls and accessibility settings are stored in `n-type/client.json` under
`$XDG_CONFIG_HOME` (or `~/.config`) on Linux, and `%APPDATA%/N-Type/client.json`
on Windows. `NTYPE_PROFILE_PATH` overrides the file path, including for tests.

## Items

Normal player fire sends one volley every 0.35 seconds (about 2.9 per second).
Each player projectile deals 2 damage, including double/triple/quadruple volleys.
Each non-boss enemy killed by a player rolls independently for at most one pickup:

| Item | Chance per kill | Effect | Time on map |
|------|-----------------|--------|-------------|
| Double nozzle | 2% | Two shots per volley | 20 s |
| Triple nozzle | 0.5% | Three shots per volley | 18 s |
| Quadruple nozzle | 0.1% | Four shots per volley | 16 s |
| Toothbrush | 15% | Restores 1 HP, up to maximum health | 12 s |
| Dental floss | 12% | Three-volley bursts for 10 seconds | 10 s |
| Golden shield | 8% | Invincibility and golden player sprite for 10 seconds | 8 s |

The remaining 62.4% of kills drop nothing. Items are displayed 80 pixels wide
(previously 48), have a 30-pixel pickup radius, and drift left at 70 px/s. They
also disappear when leaving the play area or changing levels. Bosses do not drop
items because killing them immediately ends the level.

Touch a pickup to collect it; only one player receives its effect. Nozzles replace
the current shooting pattern, including when collecting a smaller nozzle, and last
for that ship's life. Dental floss keeps the current nozzle and fires three volleys
0.1 seconds apart, followed by a 0.6-second pause, while fire is held. After ten
seconds, normal cadence returns. Another floss or shield refreshes its ten-second
effect instead of adding durations. A respawn starts with the default weapon.

The shield uses `sprites/player/tooth_golden.png` with the same 0 → 1 → 2 → 1
animation and timing. The server sends the golden state in every snapshot, so
other players and spectators see it too; the health-based sprite returns on expiry.

Definitions, drop probabilities, map lifetimes and effects are in
`assets/data/items.json`, sprite registrations in `textures.json`, and the three
additional volleys in `attack_patterns.json` (20–22). `Game::drop_item` in
`server/src/Game.cpp` performs the random roll.

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

Open **Controles** and select the device you want to use. Bindings are shared
between local player names. Check the active bindings in **Comment jouer**.
For a disconnected controller, use the menu icon or configured keyboard pause key.
If preferences do not survive a restart, check the profile save error in the panel
and the permissions of the configuration directory.

### Disconnection

Input is also a heartbeat. A client silent for more than five seconds is removed.
Connection and lobby operations retry automatically, but an interrupted session
may still require returning to the connection screen.
