# N-Type — Two networked squares

A small demonstration: one square per connected player, up to two players on
one UDP server. One player alone sees one square. A second square appears on both
clients when a second player connects. Each client controls only its own square.
The server computes all movement; clients send keys and display server positions.

## Build

Linux: git, CMake 3.25+, Ninja, a C++20 compiler and SFML's X11/OpenGL/udev headers.
Windows: git and Visual Studio 2022+ with the Desktop development with C++ workload.

```sh
./build.sh       # Linux
```

```bat
build.bat       rem Windows Command Prompt
```

The scripts build and copy both executables to the repository root. They support
`--debug`, `--clean` and `--help`. vcpkg provides SFML (window/graphics) and Asio
(network); there is no audio, external asset, ECS or menu. On Windows, keep the
copied DLLs next to the executables. In PowerShell, prefix commands with `.\`.

## Run

Start one server:

```sh
./r-type_server -p 4242
```

Start one client per player (in separate terminals or on different computers):

```sh
./r-type_client -h 127.0.0.1 -p 4242
```

On another computer, replace `127.0.0.1` with the **server computer's IPv4 address**.
Windows uses `r-type_server.exe` and `r-type_client.exe` with the same arguments.
Linux and Windows clients use the same binary protocol and can join the same server.
Allow inbound **UDP 4242** on the server firewall. Internet hosting also requires
appropriate router/hosting configuration; a LAN address is only reachable on that LAN.

- Move your square: arrows, WASD or ZQSD.
- Quit: Escape or the window close button.
- The window title identifies your player and connection status.
- A third player waits with a "Server full" status and retries automatically.
- Before connecting, or after a connection timeout, no squares are displayed.

## Files

| File | Responsibility |
|------|----------------|
| [ClientApp.cpp](client/src/ClientApp.cpp) | Window, keyboard and drawing |
| [ClientNetwork.cpp](client/src/ClientNetwork.cpp) | Send keys, receive states and reconnect |
| [main.cpp](client/src/main.cpp) | Start the client and parse options |
| [server/src/main.cpp](server/src/main.cpp) | Accept two players, compute movement and send states |
| [Protocol.hpp](shared/Protocol.hpp) | Shared constants and explicit binary encoding |
| [Options.hpp](shared/Options.hpp) | Shared command-line parsing |

The server runs a simple **single-threaded**, nonblocking loop at 60 ticks/s and
sends full states at 30/s. This is a connection/window prototype, not the previous
multithreaded R-Type engine. There is no prediction, interpolation, authentication
or general reliable-message layer. Connect is retried every 500 ms. Full state packets
are repeated, so a lost state can be replaced by the next one. Older input/state
sequences are ignored. Disconnect is sent three times; a 3-second timeout handles
lost disconnects and crashes. Movement stops after 250 ms without fresh input.

## Small binary protocol

Each UDP datagram starts with `4E 54 01 type` (literal `NT`, version 1, message type),
followed by a 32-bit connection nonce. All multibyte numbers are **little-endian**,
encoded explicitly rather than copied from C++ structs. The nonce distinguishes
connection attempts; it is not authentication.

| Type | Direction | Remaining fields after the nonce | Total bytes |
|------|-----------|---------------------------------|-------------|
| 1 Connect | Client → Server | None | 8 |
| 2 Input | Client → Server | Sequence `u32`, keys `u8` | 13 |
| 3 State | Server → Client | Server tick `u32`, recipient slot `u8`, connected mask `u8`, then `(x u16, y u16)` for slots 0 and 1 | 22 |
| 4 Disconnect | Client → Server | None | 8 |
| 5 Full | Server → Client | None | 8 |

Keys: left=1, right=2, up=4, down=8. Connected mask: bit 0 is blue, bit 1 is orange.
Coordinates for disconnected slots are zero and are not drawn. Slot numbers start
at zero. Positions refer to the square's top-left corner. Bounds are x=0…760,
y=0…560 in an 800×600 window, with 40×40 squares moving at 250 pixels/s.
Packets with invalid magic, version, length or field values are ignored.

## Verification

After building, run the black-box server checks (Python 3, standard library only):

```sh
python3 tests/network_smoke.py ./r-type_server
```

They check connection, one/two visible player slots, independent movement, malformed
packets, refusal of a third player, old inputs, disconnects and crash timeouts.
For the window, start two real clients and check that the second square appears
and disappears as the other client connects and quits.
