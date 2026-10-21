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
| `int16_t` | 2 bytes | Signed position from -32768 to 32767, stored **little-endian** |

**Little-endian** means the lower byte is written first. For example, entity ID `251` is encoded as `FB 00` (hexadecimal). Each UDP datagram contains **one complete message**. There is no additional protocol header or version number in this MVP.

### Message list

| ID | Name | Direction | Payload after type | Total size |
| ---: | --- | --- | --- | ---: |
| 1 | `ConnectRequest` | Client → Server | None | 1 byte |
| 2 | `ConnectAccept` | Server → Client | `playerId` (`uint8_t`) | 2 bytes |
| 3 | `ConnectRefuse` | Server → Client | `reason` (`uint8_t`) | 2 bytes |
| 4 | `Disconnect` | Client → Server | None | 1 byte |
| 5 | `Input` | Client → Server | `keys` (`uint8_t`) | 2 bytes |
| 6 | `EntityUpdate` | Server → Client | `id`, `x`, `y` (`uint16_t`, `int16_t`, `int16_t`) | 7 bytes |
| 7 | `EntitySpawn` | Server → Client | `id`, `kind`, `x`, `y` | 8 bytes |
| 8 | `EntityDestroy` | Server → Client | `id` (`uint16_t`) | 3 bytes |
| 9 | `WorldSnapshot` | Server → Client | `count` + `count` entities | `2 + count × 7` bytes |
| 10 | `PlayerEvent` | Server → Client | `playerId`, `event` (`uint8_t` each) | 3 bytes |
| 11 | `ClientLeft` | Server → Client | `playerId` (`uint8_t`) | 2 bytes |
| 12 | `GameStateChange` | Server → Client | `state` (`uint8_t`) | 2 bytes |

`EntitySpawn` fields are: `id` (2 bytes), `kind` (1 byte), `x` (2 bytes), `y` (2 bytes). A `WorldSnapshot` entity uses the **same 7-byte layout**, without the message type.

## 3. Values used in messages

### Player IDs and connection

- `ConnectRequest` asks the server to join the game.
- `ConnectAccept` returns a **player ID from 1 to 4**.
- `ConnectRefuse` returns `1` for **ServerFull** or `2` for **ServerError**.
- `Disconnect` is sent when a client leaves normally. `ClientLeft` tells other clients that a player has left.

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

An entity has an **ID**, a **kind**, and an **(x, y)** position. The ID identifies the same object across messages.

- `EntitySpawn`: create an entity with its initial position.
- `EntityUpdate`: change an existing entity's position.
- `EntityDestroy`: remove an entity.
- `WorldSnapshot`: send the current list of entities with their positions. The maximum is **100 entities** per snapshot.

### Player events and game states

The decoder currently accepts the following numeric values. Their meanings are **proposed MVP conventions** and must be confirmed with the gameplay developer.

| `PlayerEvent.event` | Proposed meaning | `GameStateChange.state` | Proposed meaning |
| ---: | --- | ---: | --- |
| 1 | Hit | 1 | Waiting |
| 2 | Dead | 2 | Playing |
| 3 | Respawn | 3 | Game over |

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

The decoder rejects unknown message types, wrong lengths, invalid input bits, invalid player IDs where checked, invalid entity kinds, and snapshots containing more than 100 entities. The packet-building functions do **not** all check their arguments, so callers must use valid values.

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
All 12 protocol messages passed!
```

**Before integration:** agree on message directions, event/state meanings, entity IDs, and when the server sends `EntityUpdate` versus `WorldSnapshot`.
