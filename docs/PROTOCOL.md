# N-Type R-Type Protocol — Version 1

```
Status:   Draft, implemented by r-type_server / r-type_client 0.1
Category: Game protocol over UDP
```

## 1. Introduction

This document specifies the binary protocol spoken between an R-Type client and
an R-Type server. It is complete enough to write a compatible client or server
without reading the source code; the reference implementation is
`engine/src/net/Packet.cpp` (packet layer) and
`rtype_common/src/protocol/Codec.cpp` (messages).

The key words "MUST", "MUST NOT", "SHOULD" and "MAY" are to be interpreted as
described in RFC 2119.

### 1.1 Model

- The server is **authoritative**: it runs the simulation and decides every
  outcome (movement, hits, deaths, score). Clients send their inputs and
  display what the server describes.
- All traffic is UDP. Each UDP datagram carries exactly one **packet**: a fixed
  header followed by the payload of one **message**.
- A thin reliability layer (section 3) lets some messages be delivered
  reliably; the others are fire-and-forget.

## 2. Packet layer

### 2.1 Encoding rules

- All integers are fixed-width and **little-endian**.
- `u8/u16/u32` are unsigned, `i16` is two's-complement signed.
- Structures are serialised field by field in the order listed, with no
  padding.
- A datagram MUST NOT exceed **1400 bytes** (header included).

### 2.2 Header (15 bytes)

| Offset | Size | Field          | Description |
|-------:|-----:|----------------|-------------|
| 0      | 2    | `magic`        | `0x4E54` ("NT"). On the wire: `54 4E` |
| 2      | 1    | `version`      | Packet layer version, `1` |
| 3      | 1    | `type`         | Message type (section 4) |
| 4      | 1    | `flags`        | bit 0 `RELIABLE`, bit 1 `HAS_ACK`, other bits 0 |
| 5      | 2    | `sequence`     | Sequence number of this packet |
| 7      | 2    | `ack`          | Most recent sequence received from the peer |
| 9      | 4    | `ack_bits`     | Bit *i* set = packet `ack - 1 - i` was received |
| 13     | 2    | `payload_size` | Number of payload bytes that follow (max 1385) |

### 2.3 Validation

A receiver MUST silently drop a datagram (and MUST NOT crash) when:

1. it is shorter than 15 bytes or longer than 1400 bytes;
2. `magic` or `version` does not match;
3. `payload_size` is larger than 1385, larger than the bytes actually
   present, or smaller (trailing bytes are not allowed);
4. the message type is unknown or the payload does not decode exactly
   (section 4.3).

Dropped datagrams are counted but never answered.

## 3. Reliability

Every packet is stamped by the sender with:

- `sequence`: incremented by one for every packet sent to that peer (wraps
  at 65535). Sequence `a` is more recent than `b` when
  `(a > b and a - b <= 32768) or (a < b and b - a > 32768)`.
- `ack` / `ack_bits`: the most recent sequence received from the peer and
  a bitfield of the 32 sequences before it. These fields are only meaningful
  when the `HAS_ACK` flag is set, which a peer does once it has received at
  least one packet.

A packet with the `RELIABLE` flag MUST be kept by its sender until a packet
from the peer acknowledges it (through `ack` or `ack_bits`). When no
acknowledgement arrives within the retransmission timeout
(`clamp(2 × RTT, 100 ms, 1000 ms)`), the sender resends the same payload in a
**new** packet with a fresh sequence number, and keeps waiting for any copy to
be acknowledged.

Consequently a reliable message may be received more than once; every
reliable message of this protocol is **idempotent** and receivers MUST
tolerate duplicates. Receivers drop packets whose sequence was already seen
within the 32-packet window, and unreliable packets older than that window.

Ordering is not guaranteed; section 5 describes how each message copes with it.

## 4. Messages

### 4.1 Types

| Type | Name              | Direction | Delivery   |
|-----:|-------------------|-----------|------------|
| 1    | `ConnectRequest`  | C → S     | unreliable, repeated by the client |
| 2    | `ConnectAccept`   | S → C     | reliable |
| 3    | `ConnectRefuse`   | S → C     | unreliable |
| 4    | `Disconnect`      | C → S     | unreliable, sent 3 times |
| 5    | `Input`           | C → S     | unreliable, every client tick |
| 6    | `Ping`            | C → S     | unreliable |
| 7    | `Pong`            | S → C     | unreliable |
| 8    | `EntitySpawn`     | S → C     | reliable |
| 9    | `EntityDestroy`   | S → C     | reliable |
| 10   | `WorldSnapshot`   | S → C     | unreliable |
| 11   | `PlayerEvent`     | S → C     | reliable |
| 12   | `ClientLeft`      | S → C     | reliable |
| 13   | `GameStateChange` | S → C     | reliable |

### 4.2 Shared enumerations

| Enumeration       | Values |
|-------------------|--------|
| `EntityKind`      | 0 Player, 1 Enemy, 2 PlayerMissile, 3 EnemyMissile |
| `DestroyReason`   | 0 Despawned (left the screen, player left), 1 Killed, 2 Hit (projectile consumed), 3 Expired |
| `RefuseReason`    | 1 ServerFull, 2 VersionMismatch |
| `PlayerEventType` | 1 Died, 2 Respawned |
| `LeaveReason`     | 1 Disconnected, 2 TimedOut |
| `GameState`       | 1 Waiting, 2 Running, 3 GameOver |

Input bits: bit 0 Up, 1 Down, 2 Left, 3 Right, 4 Fire. Other bits MUST be 0.

Coordinates: the world is 1280 × 720 units, origin top-left, x to the right,
y downwards. Positions are sent in **half units** (`i16`, value = position × 2,
rounded); velocities in units per second (`i16`).

### 4.3 Payload layouts

A payload MUST contain exactly the bytes listed; enumerations MUST hold one of
their listed values; counts MUST NOT exceed the stated maximum.

**ConnectRequest (1)**: `u16 protocol_version` (currently `1`).

**ConnectAccept (2)**:
`u8 slot` (0–3), `u32 player_network_id`, `u32 seed`, `u16 tick_rate` (> 0),
`u32 server_tick`.

**ConnectRefuse (3)**: `u8 RefuseReason`.

**Disconnect (4)**: empty.

**Input (5)**: `u32 client_tick`, `u8 bits`.

**Ping (6)** / **Pong (7)**: `u32 timestamp_ms`. The server echoes the value
unchanged.

**EntitySpawn (8)**: `u8 count` (≤ 64), then `count` times:
`u32 network_id`, `u8 EntityKind`, `u8 subtype`, `i16 x`, `i16 y`.
`subtype` is the player slot for players, the enemy type for enemies and the
projectile type for missiles (indices into `assets/data/game.json`).

**EntityDestroy (9)**: `u8 count` (≤ 128), then `count` times:
`u32 network_id`, `u8 DestroyReason`.

**WorldSnapshot (10)**:
`u32 server_tick`, `u32 last_processed_input`,
`u8 player_count` (≤ 4), then per player:
`u8 slot` (0–3), `u8 lives`, `u32 score`, `u8 alive` (0 or 1);
then `u16 entity_count` (≤ 80), then per entity:
`u32 network_id`, `i16 x`, `i16 y`, `i16 vx`, `i16 vy`, `u8 hp`, `u8 flags`.
Entity flags: bit 0 Hidden (dead player waiting to respawn), bit 1 Invulnerable.

**PlayerEvent (11)**: `u8 slot` (0–3), `u8 PlayerEventType`, `u32 value`
(for Died: lives left).

**ClientLeft (12)**: `u8 slot` (0–3), `u8 LeaveReason`.

**GameStateChange (13)**: `u8 GameState`.

## 5. Behaviour

### 5.1 Connection

1. The client sends `ConnectRequest` every 500 ms until it gets an answer.
2. The server answers a request from an unknown endpoint with
   `ConnectAccept`, or `ConnectRefuse` when the protocol version differs or
   no room has a free slot. A repeated request from an already accepted
   endpoint is answered again with `ConnectAccept` (idempotent).
3. Right after `ConnectAccept`, the server sends the current
   `GameStateChange` and `EntitySpawn` batches describing every existing entity.

A server hosts several rooms of up to 4 players; a client is placed in the
first room with a free slot, and a new room is opened when all are full.

### 5.2 Game loop

- The client sends one `Input` per tick (60 Hz) with an increasing
  `client_tick`. The server keeps only the newest input per client and
  reports it back in `last_processed_input`.
- The server simulates at 60 Hz. After each tick it sends, reliably,
  `EntityDestroy` then `EntitySpawn` for entities removed/created during the
  tick, then any `PlayerEvent`. About every 33 ms it sends a `WorldSnapshot`
  of all entities, split over several snapshots with the same `server_tick`
  when there are more than 80.
- Clients ignore snapshots older than the last one applied, and snapshot
  entries for unknown `network_id`s (their `EntitySpawn` may still be in
  flight). Spawning a known id and destroying an unknown id are no-ops.
- When every player has lost all lives the server sends
  `GameStateChange(GameOver)`, waits 5 seconds, removes every entity, recreates
  the players and sends `GameStateChange(Running)`.

### 5.3 Leaving and timeouts

- A client that quits sends `Disconnect` three times.
- Each side considers the other gone after **5 seconds** without any packet.
  The client sends a `Ping` every second so this never happens to a live
  client.
- When a player leaves or times out, the server removes its ship
  (`EntityDestroy`), notifies the remaining players with `ClientLeft`, and the
  game goes on.

## 6. Security considerations

- Every field read is bounds-checked; malformed input is dropped, never
  trusted, and never crashes the receiver (section 2.3). Counts are capped
  before any allocation.
- The server only accepts `ConnectRequest` from unknown endpoints; any other
  message from them is ignored, and clients only accept packets from the
  server they connected to.
- Clients never decide outcomes, so a modified client can at most send
  inputs a regular player could send.
- The protocol has no authentication or encryption; it is meant for trusted
  networks.
