# N-Type — Network Protocol

## 1. Purpose

This document describes how the **client and server exchange game messages** in the N-Type MVP.

- **UDP** is used to send and receive messages.
- Messages use a **binary format**, not text or JSON.
- The **server decides** what happens in the game; clients send player actions and display server updates.
- The protocol code **creates and reads messages**. It does **not** open sockets or call `send` / `receive`.

The same protocol files must be used by both the client and server.

## 2. Message format

Every message begins with **1 byte identifying its type**, followed by the fields required for that message.

| Data type | Size | Notes |
| --- | ---: | --- |
| `uint8_t` | 1 byte | Number from 0 to 255 |
| `uint16_t` | 2 bytes | Unsigned ID, stored **little-endian** |
| `int32_t` | 4 bytes | Signed world position, stored **little-endian** |
| `str` | 1 + length bytes | One `uint8_t` length, then that many ASCII bytes from 32 to 126 (printable). Never empty |
| `code` | 4 bytes | Room code: 4 characters from `23456789ABCDEFGHJKLMNPQRSTUVWXYZ`, no length byte |

**Little-endian** means the lower byte is written first. For example, entity ID `251` is encoded as `FB 00` (hexadecimal). Each UDP datagram contains **one complete message**. There is no additional protocol header or version number in this MVP.

### Message list

| ID | Name | Direction | Payload after type | Total size |
| ---: | --- | --- | --- | ---: |
| 1 | `ConnectRequest` | Client → Server | `name` (`str`, 1–16) | variable |
| 2 | `ConnectAccept` | Server → Client | None | 1 byte |
| 3 | `ConnectRefuse` | Server → Client | `reason` (`uint8_t`) | 2 bytes |
| 4 | `Disconnect` | Client → Server | None | 1 byte |
| 5 | `Input` | Client → Server | `keys` (`uint8_t`) | 2 bytes |
| 6 | `EntityUpdate` | Server → Client | `id`, `x`, `y` (`uint16_t`, `int32_t`, `int32_t`) | 11 bytes |
| 7 | `EntitySpawn` | Server → Client | `id`, `kind`, `x`, `y` | 12 bytes |
| 8 | `EntityDestroy` | Server → Client | `id` (`uint16_t`) | 3 bytes |
| 9 | `WorldSnapshot` | Server → Client | `frame` (`uint32_t`), `part`, `parts` (1–32), `count` (0–90) (`uint8_t` each), then `count` entities | `8 + count × 14` bytes |
| 10 | `PlayerEvent` | Server → Client | `playerId`, `event` (`uint8_t` each) | 3 bytes |
| 11 | `ClientLeft` | Server → Client | `playerId` (`uint8_t`) | 2 bytes |
| 12 | `GameStateChange` | Server → Client | `state` (`uint8_t`) | 2 bytes |
| 13 | `LobbyListRequest` | Client → Server | None | 1 byte |
| 14 | `LobbyList` | Server → Client | `count` (`uint8_t`, ≤ 32), then per lobby: `code`, `name` (`str`, 1–24), `members` (1–4), `state` (`uint8_t` each) | variable |
| 15 | `LobbyCreate` | Client → Server | `name` (`str`, 1–24) | variable |
| 16 | `LobbyJoin` | Client → Server | `code` | 5 bytes |
| 17 | `LobbyRefuse` | Server → Client | `reason` (`uint8_t`) | 2 bytes |
| 18 | `LobbyLeave` | Client → Server | None | 1 byte |
| 19 | `LobbyState` | Server → Client | `code`, `name` (`str`, 1–24), `state`, `level`, `result`, `yourSlot`, `leaderSlot`, `count` (1–4) (`uint8_t` each), then per member: `slot` (1–4), `role` (`uint8_t` each), `score` (`uint32_t`), `name` (`str`, 1–16) | variable |
| 20 | `LobbyRename` | Client → Server | `name` (`str`, 1–24) | variable |
| 21 | `LobbyStart` | Client → Server | None | 1 byte |

`EntitySpawn` fields are: `id` (2 bytes), `kind` (1 byte), `x` (4 bytes), `y` (4 bytes).

A `WorldSnapshot` or `EntityDied` entity takes **14 bytes**: `id` (`uint16_t`), `kind`, `type`, `hp`, `hits` (`uint8_t` each), `x`, `y` (`int32_t` each).

| ID | Name | Direction | Payload after type | Total size |
| ---: | --- | --- | --- | ---: |
| 22 | `EntityDied` | Server → Client | one entity (14 bytes) | 15 bytes |

## 3. Values used in messages

### Connection

- `ConnectRequest` carries the player's name and asks the server to accept the client. It is repeated every 500 ms until answered.
- `ConnectAccept` has no payload. Player IDs (1–4) are **lobby slots**, given by `LobbyState`.
- `ConnectRefuse` returns `1` for **ServerFull** (the server already holds 128 clients) or `2` for **ServerError**.
- `Disconnect` is sent when a client leaves normally. `ClientLeft` tells the other members of a playing lobby that a player (slot) has left.
- `Input` is sent every frame on every screen and is also the heartbeat: a client silent for 5 s is dropped. Its keys only move a player of a running game.

### Player inputs

| Input | Value |
| --- | ---: |
| Up | 1 |
| Down | 2 |
| Left | 4 |
| Right | 8 |
| Shoot | 16 |

Inputs can be combined. For example, **Right + Shoot = 24**, sent as `05 18` in hexadecimal (type `5`, keys `24`). A value of `0` means no key is pressed. Only the five lowest bits may be used.

The server identifies which player sent an `Input` using its network session, **not** an ID inside the input packet.

### Entity types

| `kind` | Meaning |
| ---: | --- |
| 1 | Player |
| 2 | Enemy |
| 3 | Player projectile |
| 4 | Enemy projectile |
| 5 | Item |

An entity has an **ID**, a **kind**, and an **(x, y)** position. The ID identifies the same object across messages.

- **IDs**: players are their lobby slot (1–4); every other entity has an ID from 5.
- **Positions**: a player's (x, y) is the top-left corner of its 40×40 box; any other entity's is its centre.
- **`type`** says which one: the enemy type (`enemies.json`), the attack pattern of a projectile (`attack_patterns.json`), or the item type (`items.json`); 0 for players. The client picks the sprite from it.
- **`hp`** is the health left, in percent of the maximum (0–100), for players and enemies; 0 for the others.
- **`hits`** counts the hits the entity took (players and enemies), wrapping from 255 to 0. When it changes between two frames, the client shows an impact (flash and sound). A counter rather than a one-off event, so a lost packet cannot hide a hit.

- `EntitySpawn`: create an entity with its initial position.
- `EntityUpdate`: change an existing entity's position.
- `EntityDestroy`: remove an entity.
- `WorldSnapshot`: send every entity of one game frame. A frame is split into **parts** of at most 90 entities (about 1,270 bytes, safe on any network): each part carries the frame number, its index and the number of parts. The client shows a frame once all its parts arrived, keeps the previous frame meanwhile, and drops parts of a frame older than the one shown or being built. Frame numbers only grow within a lobby.
- `EntityDied`: an enemy or a player died at (x, y), sent once when it happens so the client can show the explosion. It is best effort: if it is lost, the entity still disappears from the next frame.

### Player events and game states

The decoder currently accepts the following numeric values. Their meanings are **proposed MVP conventions** and must be confirmed with the gameplay developer.

| `PlayerEvent.event` | Proposed meaning | `GameStateChange.state` | Proposed meaning |
| ---: | --- | ---: | --- |
| 1 | Hit | 1 | Waiting |
| 2 | Dead | 2 | Playing |
| 3 | Respawn | 3 | Game over |

### Lobbies

One server hosts up to 32 lobbies; each lobby runs its own game for up to 4 members.

- `LobbyRefuse.reason`: `1` NotFound (no lobby with this code), `2` Full (4 members), `3` TooManyLobbies.
- Lobby `state`: `1` Waiting, `2` Playing (same values as `GameStateChange`).
- Member `role`: `1` Player, `2` Spectator (joined during a game, or died; becomes a player at the next level or when the lobby returns to waiting). When no player is left, or the last level's boss dies, the lobby returns to waiting.
- There is no map choice: the game plays map N at level N.
- `LobbyState` is built for each recipient: `yourSlot` is the slot of the member who receives it. The leader is the member present the longest.
- Member `score` (`uint32_t`, little-endian) counts the member's points in the current game. It returns to 0 when the leader starts a game and is kept after game over, so the waiting room shows the last result.
- Picking a lobby from the list sends `LobbyJoin` with its code.
- `WorldSnapshot` is sent every tick to the members of each playing lobby only; player entity IDs are the lobby slots.
- `LobbyState` `level` is the level being played, from 1; once a game ended, the level it reached; 0 before any game.
- `LobbyState` `result` says how the last game ended: `0` none (no game ended yet, or one is running), `1` defeat (every player died, or the game was stopped), `2` victory (the last boss died). With the members' scores, which are kept after a game, it fills the results screen; both reset when the leader starts again.

**Reliability: the state is re-sent, requests are repeated.** There are no acknowledgements:

- While browsing, the client sends `LobbyListRequest` every second; the server answers with the whole `LobbyList`.
- The server sends every lobby member its `LobbyState` 10 times per second, and once right after each request.
- The client repeats each request (`LobbyCreate`, `LobbyJoin`, `LobbyRename`, `LobbyStart`) every 250 ms until a `LobbyState` shows its effect, or until `LobbyRefuse` arrives.
- `LobbyLeave` is repeated until a `LobbyList` arrives: the server always answers it with the list.
- Every request is safe to repeat. A member sending `LobbyCreate` or `LobbyJoin` gets its current lobby's state back. Requests from non-leaders, or invalid in the lobby's state, are ignored: the next `LobbyState` shows nothing changed.

## 4. Example: moving and shooting

1. The player holds **Right** and **Shoot**.
2. The client creates the two-byte message `05 18`.
3. The network code sends these bytes using UDP.
4. The server decodes the message and reads `type = Input`, `value = 24`.
5. The server processes the action and sends updated entities to the clients.

```cpp
// Client: prepare bytes for the UDP send function
uint8_t keys = 8 | 16; // Right + Shoot
std::vector<uint8_t> data = protocol::makeInput(keys);

// Server: decode bytes returned by the UDP receive function
auto packet = protocol::decode(receivedData);
if (packet && packet->type == protocol::MessageType::Input) {
    uint8_t pressedKeys = packet->value;
    // Pass pressedKeys to the server's game logic.
}
```

`receivedData` is supplied by the network code; the protocol library does not receive UDP packets itself.

## 5. Files and responsibilities

| File | Purpose |
| --- | --- |
| `MessageType.hpp` | Defines message IDs, refusal reasons and input keys |
| `Protocol.hpp` | Defines `Entity`, `Packet` and the available functions |
| `Protocol.cpp` | Converts messages into bytes and bytes back into messages |
| `tests/protocol_test.cpp` | Tests every message type and several invalid packets |

- **`make...()` functions:** create a byte array ready to send.
- **`decode(data)`:** returns a `Packet` if the message is valid, or `std::nullopt` if it is invalid.
- In a decoded `Packet`, `id`, `value`, `x`, `y` and `entities` are only meaningful when used by that message type.

## 6. Validation and current limits

The decoder rejects unknown message types, wrong lengths, invalid input bits, invalid player IDs where checked, invalid entity kinds, snapshot parts with more than 90 entities or an invalid part index, health above 100, empty, too long or non-printable strings, invalid room codes, lists of more than 32 lobbies and lobby states with 0 or more than 4 members. The string, code and lobby builders throw `std::invalid_argument` on invalid values; the other builders do **not** check their arguments, so callers must use valid values.

**Not handled by this protocol module:** UDP socket management, retransmission of lost messages, ordering of delayed messages, authentication, and recovery from a lost spawn/destroy event. These are integration concerns for the network/gameplay team. UDP does not guarantee delivery or order; sending snapshots periodically can help refresh positions, but does not replace a complete reliability strategy.

## 7. Test the protocol

From the project root, with the paths shown above:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  common/protocol/Protocol.cpp tests/protocol_test.cpp \
  -o protocol_test
./protocol_test
```

Expected output:

```text
All 22 protocol messages passed!
```

**Before integration:** agree on message directions, event/state meanings, entity IDs, and when the server sends `EntityUpdate` versus `WorldSnapshot`.
