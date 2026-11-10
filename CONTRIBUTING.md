# Contributing to N-Type

Read [Architecture](docs/ARCHITECTURE.md) before changing runtime design and the
[Content Guide](docs/CONTENT_GUIDE.md) before adding JSON-driven content.

## Development build

```sh
./build.sh --debug
```

The build uses CMake presets and vcpkg for Asio, fmt, nlohmann/json, and SFML 2.6.
Keep SFML 2.6 compatibility. List direct CMake presets with:

```sh
cmake --list-presets
```

## Design rules

- Keep the server authoritative for positions, damage, roles, scores, and levels.
- Prefer JSON for content already supported by existing systems.
- Keep components data-only and behavior in systems.
- Preserve JSON and protocol IDs unless making an intentional migration.
- Update encoder, decoder, protocol tests, and protocol documentation together.
- Fail early with an actionable error for invalid loaded content.
- Keep warning-clean C++20 and use RAII for owned resources.

## Test

```sh
ctest --test-dir build/client-linux-debug --output-on-failure
ctest --test-dir build/client-linux-debug -R protocol_test --output-on-failure
python3 tests/network_smoke.py ./r-type_server
```

| Test | Coverage |
| --- | --- |
| `protocol_test` | Encoding, decoding, validation, multipart snapshots |
| `engine_test` | Data loading, factories, systems, combat, items |
| `game_test` | Simulation, waves, snapshots, score events |
| `lobby_test` | Membership, leadership, spectators, lifecycle |
| `client_network_test` | Retries, transitions, snapshot assembly |
| `network_smoke.py` | Real UDP behavior and timeouts |

For UI/audio work, manually test several window sizes, Escape, F11, leave/rejoin,
spectator mode, and both victory and defeat.

## Generate the API reference

The repository includes a `Doxyfile`. Once Doxygen and Graphviz are installed:

```sh
doxygen Doxyfile
```

Open `docs/api/html/index.html` in a browser. Generated files are ignored by Git.
See [Generate the API Documentation](docs/DOXYGEN.md) for complete Linux and
Windows installation, generation, viewing, cleanup, and troubleshooting commands.

## Protocol changes

There is no version negotiation, so incompatible changes must be atomic:

1. Update types and enums in `MessageType.hpp`.
2. Update packet data/declarations in `Protocol.hpp`.
3. Implement strict encode/decode logic in `Protocol.cpp`.
4. Reject invalid ranges and trailing bytes.
5. Add byte-exact success and malformed-input tests.
6. Update `docs/PROTOCOL_v2.md` and architecture notes.

Prefer a new message type to changing an existing layout. Keep UDP datagrams below
common path MTUs; snapshot parts are approximately 1,268 bytes for this reason.

## Pull-request checklist

- Relevant Debug/platform builds succeed and all tests pass.
- New behavior has focused regression tests.
- Public, data, and protocol behavior is documented in English.
- No build output, local vcpkg tree, credentials, or editor state is included.
- New assets have known redistribution rights and follow `assets/README.md`.
- Unrelated local changes are not included.

Bug reports should include OS, compiler, preset, exact command, logs, reproduction
steps, and whether networking is local, LAN, or internet.
