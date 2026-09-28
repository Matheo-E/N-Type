# Cross-Platform Compatibility Guide (for AI agents)

This file exists to give AI coding agents consistent rules for keeping this project
buildable and runnable identically on Linux and Windows. Follow these unless a
specific task instruction overrides them. If unsure, prefer the option that avoids
platform-specific code entirely.

## 1. Build System

- Use CMake as the single build system. Never hand-maintain a separate Makefile and
  Visual Studio project in parallel — CMake generates both.
- Keep `CMakeLists.txt` free of Linux-only assumptions (no hardcoded `/usr/...`
  paths, no shelling out to `pkg-config` as the only lookup path).
- Use `find_package()` / vcpkg toolchain integration for dependencies, not manual
  `-I`/`-L` flags that only exist on one OS.
- Target a specific C++ standard explicitly (`set(CXX_STANDARD 20)` or similar) so
  MSVC and GCC/Clang compile the same language version.

## 2. Dependency Management

- Use vcpkg (manifest mode, `vcpkg.json`) to fetch and build third-party
  dependencies. Do not rely on `apt install` packages that have no Windows
  equivalent, or on manually downloaded `.lib`/`.dll` files.
- Any dependency added must have a maintained Windows build path. Verify this
  before adding it, not after CI fails.

## 3. Graphics & Audio

- Use SFML for windowing, rendering, input, and audio. Never call platform APIs
  directly (no Win32/X11/Wayland calls, no `windows.h` in game logic).
- Isolate any unavoidable platform-specific code behind a small interface in a
  dedicated `platform/` folder — never scatter `#ifdef _WIN32` through gameplay
  code.

## 4. Networking

- Use Asio (standalone or via Boost) for all sockets. Never call BSD sockets or
  Winsock directly.
- If Asio is ever bypassed for some reason, remember Windows requires
  `WSAStartup`/`WSACleanup` around socket use — Linux does not. This is exactly the
  kind of thing to avoid needing by staying on Asio.
- Define the wire protocol explicitly and serialize field-by-field (fixed-width
  types, explicit byte order) instead of `memcmp`/`memcpy`-ing structs over the
  network. Struct padding and alignment differ between MSVC and GCC, so raw struct
  serialization will corrupt data between a Windows client and a Linux server (or
  vice versa).
- Prefer fixed-width integer types (`uint32_t`, `int16_t`, etc.) over `int`/`long`
  in any serialized/network-facing struct — `long` is 4 bytes on Windows (LLP64) and
  8 bytes on Linux (LP64).

## 5. Filesystem & Paths

- Use `std::filesystem::path` for all paths. Never build paths via string
  concatenation with a hardcoded `/` or `\`.
- Linux filesystems are case-sensitive; Windows (NTFS) is case-insensitive but
  case-preserving. Keep `#include` and asset-loading filenames byte-for-byte
  identical to the actual file's case — a mismatch works on Windows and silently
  fails on Linux.
- Avoid Windows reserved device names for any generated file (`CON`, `PRN`, `AUX`,
  `NUL`, `COM1`-`COM9`, `LPT1`-`LPT9`) — they can't be created as regular files on
  Windows.
- Don't assume a `HOME` environment variable; use
  `std::filesystem::current_path()` or an explicit config path, or check both
  `HOME` (Linux) and `USERPROFILE` (Windows) if a user directory is genuinely
  needed.

## 6. Timing, Sleeping & Threads

- Use `std::chrono` for all time measurement, never `<time.h>` platform-specific
  calls or `QueryPerformanceCounter`/`clock_gettime` directly.
- Use `std::this_thread::sleep_for` instead of `usleep`/`Sleep`.
- Use `std::thread`/`std::jthread`, `std::mutex`, etc. from the standard library
  instead of pthreads or the Win32 thread API directly.

## 7. Text, Line Endings & Encoding

- Add a `.gitattributes` that normalizes line endings (`* text=auto`) so files
  don't flip between LF and CRLF depending on which OS commits them.
- When reading text files line-by-line, don't assume a bare `\n` — handle a
  trailing `\r` gracefully (or open in a mode that normalizes it).
- Assume the console/terminal encoding may differ (UTF-8 on Linux terminals,
  historically a different code page on Windows). Avoid relying on non-ASCII
  characters in console output; if needed, set the Windows console to UTF-8
  explicitly at startup.

## 8. Compiler Portability

- Stick to standard C++; avoid compiler-specific extensions (GCC's
  `__attribute__((...))`, MSVC's `__declspec(...)`) unless wrapped in a portable
  macro defined once in a shared header.
- Don't rely on GCC/Clang being lenient about things MSVC rejects (e.g. missing
  `typename` on dependent types, certain implicit conversions). Treat MSVC as the
  stricter reference compiler when in doubt.
- If building a shared library (`.so`/`.dll`), remember Windows requires explicit
  `__declspec(dllexport)`/`dllimport` on exported symbols; Linux does not. Wrap this
  in a portable `MYLIB_API` macro rather than writing it inline everywhere.

## 9. Process & System Calls

- Never call `fork()`, `exec()`, or POSIX signal handling directly — Windows has no
  equivalent. If subprocess/signal handling is genuinely needed, isolate it behind
  a platform abstraction, or prefer a portable library.
- Never call `system()` with a shell command string that assumes a POSIX shell
  (`ls`, `rm`, `/bin/sh` syntax) — it will fail or do the wrong thing under
  `cmd.exe`.

## 10. Continuous Integration

- Run a CI build matrix covering at least `ubuntu-latest` and `windows-latest` on
  every push/PR. A change that only builds on Linux is not done.
- Treat a Windows CI failure with the same priority as a Linux one — don't merge
  with one leg red "to fix later."

## 11. General Agent Behavior

- When adding any new include, library call, or system interaction, ask: "does
  this exist unchanged on both Linux and Windows?" If not, find the portable
  standard-library or Asio/SFML equivalent before reaching for `#ifdef`.
- `#ifdef _WIN32` / `#ifdef __linux__` branches are a last resort, not a first
  option. If one is unavoidable, confine it to the smallest possible scope inside
  `platform/`, with both branches implemented in the same change — never add one
  platform's branch and leave the other as a TODO.
- Don't assume the development machine's OS matches the target/CI OS. Code should
  be written to be correct on both, not just tested on whichever OS is at hand.
