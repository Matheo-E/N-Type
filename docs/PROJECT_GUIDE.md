# Project Guide

The original all-in-one guide has been split into focused, current documents:

- [User Guide](USER_GUIDE.md): build, run, host, play, and troubleshoot.
- [Architecture](ARCHITECTURE.md): client/server design, ECS, data flow, and boundaries.
- [Content Guide](CONTENT_GUIDE.md): add enemies, attacks, flight patterns, items,
  levels, textures, components, and systems.
- [Protocol v2](PROTOCOL_v2.md): exact UDP wire format and reliability behavior.
- [API Documentation](DOXYGEN.md): generate and view Doxygen on Linux or Windows.
- [Contributing](../CONTRIBUTING.md): workflow, tests, conventions, and review checklist.
- [Main README](../README.md): project overview and quick start.

These documents describe the current networked game. `src/main.cpp` is a legacy
standalone demo and is not built by root CMake; use `sandbox/main.cpp` and the
`ntype_sandbox` target for engine-only experiments.
