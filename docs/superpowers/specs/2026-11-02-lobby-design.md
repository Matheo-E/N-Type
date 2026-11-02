# Lobby — Design

Date: 2026-11-02 · Branch: `Lobby` (from `origin/login_page`)

> **Amended 2026-11-02: map selection removed.** There is no map choice: the
> game plays map N at level N. Every `map` field, `LobbySetMap`, `set_map`,
> `kMapCount` and the map cards below no longer exist, and `LobbyStart` is
> message **21**. [PROTOCOL_v2.md](../../PROTOCOL_v2.md) is the current wire format.

## 1. Goal

A player starts the client, chooses a name, then either **creates** a lobby or
**joins** one. One server hosts many lobbies; **each lobby runs its own game**.

- The creator is the **leader**: starts the game, renames the lobby, changes the
  map (stored only; maps are not implemented yet).
- Joining: pick a lobby from a list of current lobbies (waiting or playing), or
  type the **room code** shown on the waiting room.
- Joining a lobby that is playing makes you a **spectator**: you watch, and you
  become a player when a new level starts or when the game returns to the
  waiting room.
- If the leader leaves, the **longest-present member is promoted**.
- When everyone is game over, the lobby goes **back to the waiting room**; the
  leader starts again.
- Menu art does not exist yet: the UI is placeholder shapes, built so textures
  can be added later without touching screen logic.

Builds on the teammate's `login_page` launcher: its connection page
(name/host/port) and F11 fullscreen/letterbox are kept; its mock lobby pages are
rebuilt on real server data.

## 2. Rules and limits

| Item | Value |
|---|---|
| Room code | 4 characters from `23456789ABCDEFGHJKLMNPQRSTUVWXYZ`, unique on the server, typed case-insensitively |
| Player name | 1–16 printable ASCII characters (32–126); duplicates allowed |
| Lobby name | 1–24 printable ASCII characters; default `"<player>'s lobby"` |
| Members per lobby | 4 (players + spectators); a spectator reserves a slot |
| Lobbies per server | 32 |
| Clients per server | 128 |
| Maps | ids `0 .. kMapCount-1`, `kMapCount = 3` (placeholders Blue/Green/Red) |

- An empty lobby is deleted.
- Rename and map change only while the lobby is waiting.
- The teammate's "Play solo" button and 2–4 player-count selector are removed
  (create + start alone covers solo; every lobby takes 4).

## 3. Protocol

Approach: **re-send the state**, like snapshots. No acknowledgements.

- While browsing, the client sends `LobbyListRequest` every second; the server
  answers with the full `LobbyList`.
- While in a lobby, the server sends each member its `LobbyState` every 6 ticks
  (10 Hz), in waiting and in playing state.
- The client re-sends each lobby request every 250 ms until a `LobbyState`
  shows its effect or a `LobbyRefuse` arrives; the server makes every request
  safe to repeat (§4.3).

### 3.1 String type

`uint8_t` length, then that many bytes, each 32–126. The decoder rejects empty
strings, strings over the field's limit and other bytes. Room codes are a fixed
4 bytes (no length), validated against the code alphabet.

### 3.2 Changed messages

| ID | Name | Change |
|---:|---|---|
| 1 | `ConnectRequest` | Payload `name` (string, 1–16) |
| 2 | `ConnectAccept` | No payload (player ids now belong to a lobby) |
| 3 | `ConnectRefuse` | `ServerFull` = the server holds 128 clients |

### 3.3 New messages

| ID | Name | Dir | Payload |
|---:|---|---|---|
| 13 | `LobbyListRequest` | C→S | none |
| 14 | `LobbyList` | S→C | `count` u8 (≤ 32), then per lobby: `code` (4 bytes), `name` (str), `map` u8, `members` u8 (1–4), `state` u8 |
| 15 | `LobbyCreate` | C→S | `name` (str), `map` u8 |
| 16 | `LobbyJoin` | C→S | `code` (4 bytes) |
| 17 | `LobbyRefuse` | S→C | `reason` u8: 1 NotFound, 2 Full, 3 TooManyLobbies |
| 18 | `LobbyLeave` | C→S | none |
| 19 | `LobbyState` | S→C | `code` (4), `name` (str), `map` u8, `state` u8, `yourSlot` u8, `leaderSlot` u8, `count` u8 (1–4), then per member: `slot` u8 (1–4), `role` u8, `name` (str) |
| 20 | `LobbyRename` | C→S | `name` (str) |
| 21 | `LobbySetMap` | C→S | `map` u8 |
| 22 | `LobbyStart` | C→S | none |

- Lobby `state`: 1 Waiting, 2 Playing (same values as `GameStateChange`).
- Member `role`: 1 Player, 2 Spectator.
- Picking a lobby from the list sends `LobbyJoin` with its code.
- `LobbyState` is built per recipient (`yourSlot`).
- The largest `LobbyList` is about 1 KB and fits one datagram.
- `Input` stays the heartbeat on every screen; the server ignores its keys
  unless the sender is a player in a running game.
- `WorldSnapshot` is unchanged but sent **per lobby**, to that lobby's members.
  Entity ids are lobby slots 1–4 (colours keep working).

`Packet` gains `text`, `code`, `lobbies` and `members` fields; `Protocol.hpp`
gains `LobbyInfo` and `LobbyMember` structs and one `make...` function per new
message.

## 4. Server

### 4.1 Files

- **`Server.{hpp,cpp}`** — network thread + 60 Hz loop. Holds **clients**:
  `endpoint → Client { name, last_seen, input, lobby code? }`, up to 128.
  Decodes packets, handles connect/disconnect/5 s timeout (leaving also leaves
  the lobby), forwards lobby requests to `Lobbies`.
- **`Lobbies.{hpp,cpp}`** — all lobbies by code, up to 32. Creates lobbies with
  a fresh random code, finds by code, deletes empty lobbies, builds the list.
- **`Lobby.{hpp,cpp}`** — one lobby's rules, no networking: code, name, map,
  state, members ordered by arrival `{ client, slot 1–4, role }`; the leader is
  the first member.
- **`Game.{hpp,cpp}`** — today's movement code moved out of `Server`: one
  engine `World` per lobby, ships by slot, `apply_input`, `step(dt)`,
  `snapshot()`.

`Lobby`, `Lobbies` and `Game` form the `ntype_server_core` static library,
linked by `r-type_server` and `lobby_test`.

### 4.2 Lobby operations

- `join` — lowest free slot; player if waiting, spectator if playing; refused
  (`Full`) at 4 members.
- `leave` — frees the slot and removes its ship; if no players remain while
  playing, `end_game()`.
- `rename`, `set_map` — leader only, waiting only.
- `start` — leader only, waiting only; everyone becomes a player, a `Game` is
  created with one ship per member.
- `on_level_start()` — spectators become players and get a ship.
- `end_game()` — back to waiting, game destroyed, everyone a player.

### 4.3 Repeated and invalid requests

- `LobbyCreate` / `LobbyJoin` from a client already in a lobby: send that
  lobby's state again (no second lobby, no move).
- `LobbyJoin` to an unknown code: `LobbyRefuse NotFound`. Full: `Full`.
- `LobbyCreate` with 32 lobbies: `TooManyLobbies`.
- `LobbyLeave` from a client not in a lobby: ignored.
- Requests from non-leaders, or invalid for the current state (e.g. `Start`
  while playing, bad map id): ignored; the next `LobbyState` shows no change.
- Lobby requests from unknown endpoints (no `ConnectRequest`): ignored.

### 4.4 Tick

1. Read packets. 2. Drop timed-out clients. 3. Step every playing lobby's game.
4. Send each playing lobby's snapshot to its members. 5. Every 6 ticks, send
each member its `LobbyState`.

### 4.5 Debug commands

Levels and game over do not exist yet. A small thread reads stdin lines:

- `level <code>` → `on_level_start()` on that lobby
- `gameover <code>` → `end_game()` on that lobby

Lines are queued to the game thread (same `ThreadSafeQueue` pattern); the
gameplay team will later call the same two functions.

## 5. Client

### 5.1 `ClientNetwork` — the session

Created once host, port and name are confirmed; lives until the app quits, so
the same session goes from menus into the game.

Phases `Connecting → Browsing → InLobby`:

- **Connecting** — `ConnectRequest(name)` every 500 ms; after 3 s without an
  answer, error "No server answered on host:port" (keeps retrying).
- **Browsing** — `LobbyListRequest` every second; exposes `lobbies()`.
- **InLobby** — exposes `lobby()` from the latest `LobbyState`; in a playing
  lobby, also the snapshot players.

Actions: `create(name, map)`, `join(code)`, `leave()`, `rename(name)`,
`set_map(id)`, `start()`. Each becomes the single **pending request**, re-sent
every 250 ms until a `LobbyState` reflects it or a `LobbyRefuse` sets
`error()`; a new action replaces it; after 3 s, error "Server did not respond".

`Input` is sent every frame (keys = 0 outside the game). 5 s without any
message from the server: reset to Connecting, status "Connection lost".

### 5.2 `ClientApp` and screens

One window for the whole app (replaces "launcher window, then game window").
`main.cpp` builds `ClientApp(defaults)`. F11 fullscreen and letterbox apply to
every screen.

The current screen follows the network phase:

| Phase | Screen |
|---|---|
| no session | Connect |
| Browsing | Lobby list, or Create when opened |
| InLobby, waiting | Waiting room |
| InLobby, playing | Game (spectator banner if spectator) |

- In game, Esc leaves the lobby (back to the list). Elsewhere Esc goes back one
  screen. Closing the window quits (`Disconnect` ×3).
- `server_is_reachable()` is removed.

## 6. UI structure

Under `client/src/ui/` and `client/src/screens/`.

- **`ui/Theme.hpp`** — the only place that knows the look: font (system-font
  fallback from the launcher), colours, text sizes, padding, 1280×720 menu
  view. Later: optional textures (button, panel 9-slice, background) that
  widgets use when set.
- **`ui/Widgets.{hpp,cpp}`** — `Label`, `Button`, `TextField` (focus, typing,
  max length, allowed characters), `ListView` (selectable rows, scroll),
  `Panel`. Each has `bounds`, `handle(event)`, `draw(target, theme)`. No game
  logic.
- **Screens** — `Screen { handle(event), update(), draw() }`. A screen reads
  `ClientNetwork` and calls its actions; it never sends packets.
  - `ConnectScreen` — the launcher's page on widgets: Name, Host, Port,
    Continue.
  - `LobbyListScreen` — list (name, code, `n/4`, map, Waiting/Playing), Join
    selected (double-click also joins), Code field + Join, Create lobby.
  - `CreateScreen` — lobby name (prefilled), 3 map cards, Create.
  - `WaitingRoomScreen` — lobby name, large room code, map preview, members
    (colour, name, ★ leader, "spectator"); leader also gets rename field, map
    cards and Start; everyone gets Leave.
  - `GameScreen` — today's `sync_world` + `render`, plus the banner
    "Spectating — you'll join at the next level".
- Each screen positions its widgets in one `layout()` function, relative to
  the menu view.

## 7. Testing

- **`protocol_test`** (extended): round-trip of every new message and of the
  new `ConnectRequest`/`ConnectAccept`; rejection of empty, too long and
  non-printable strings, bad codes, truncated `LobbyList`/`LobbyState`, bad
  role/state, more than 32 lobbies or 4 members.
- **`lobby_test`** (new, on `ntype_server_core`): unique codes from the
  alphabet; lowest free slot; 5th member refused; leader is first member and is
  promoted on leave; empty lobby deleted; leader-only and waiting-only
  operations; join while playing → spectator; `on_level_start` promotes;
  `end_game` → waiting with everyone a player; last player leaving a game ends
  it; repeated create/join/leave give the same result.
- **`tests/network_smoke.py`** (updated, standard library only): existing
  checks adapted (connect with a name, 129th client refused, malformed packets,
  disconnect, timeout); A creates, B sees it in the list and joins by code,
  both see A as leader; B's start ignored, A's start works, both receive
  snapshots and a client outside the lobby receives none; C joins during the
  game as spectator; `level`/`gameover` stdin commands; A disconnects → B
  leader; two lobbies playing at once each receive only their own snapshots.
- **Manual**: 3 real clients — create → join from list → join by code → start
  → spectate → Esc back to list.

## 8. Docs

- `docs/PROTOCOL_v2.md`: string type, changed and new messages, re-send rules.
- `README.md`: message table, run instructions, files table, debug commands.

## 9. Out of scope

Actual maps, levels, game over detection, enemies, private/password lobbies,
kicking players, chat, a reliable-message layer, real menu art.
