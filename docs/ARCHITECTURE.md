# C++ Architecture Guide (for AI agents)

This file exists to give AI coding agents consistent rules when writing or modifying
C++ code in this project. Follow these unless a specific instruction in the task
overrides them. If unsure, prefer the simpler, more explicit option.

## 1. Ownership & Memory

- Prefer stack allocation and value semantics by default. Reach for heap allocation
  only when lifetime genuinely needs to outlive scope, or the object is too large/
  polymorphic to live on the stack.
- Never use raw `new`/`delete` directly. Use `std::unique_ptr` for single ownership,
  `std::shared_ptr` only when shared ownership is truly required (not as a default).
- Raw pointers and references are fine for *non-owning* access. A function taking a
  raw pointer/reference never takes ownership and never deletes it.
- Avoid manual resource management (files, sockets, locks). Wrap them in RAII types.
- Follow the Rule of Zero: don't hand-write a copy constructor, copy assignment,
  move constructor, move assignment, or destructor unless the class does genuinely
  novel resource ownership. Let the compiler generate them — it's less code to
  maintain and avoids subtle bugs when new members are added later.

## 2. Error Handling

- Use exceptions for exceptional, unrecoverable-at-this-level errors (invalid input
  from outside the program's control, corrupted state).
- Use return values (`std::optional`, `std::expected` if available, or a result
  struct) for expected, recoverable failure cases (e.g. "file not found" when
  checking is normal control flow).
- Never use error codes and exceptions interchangeably in the same subsystem — pick
  one convention per layer and be consistent.
- Never silently swallow errors. If you catch an exception, either handle it
  meaningfully or rethrow.

## 3. Module & Layer Boundaries

- Organize code in layers with a one-directional dependency flow, e.g.:
  `core/` (domain logic, no I/O) → `services/` (business logic, orchestration) →
  `interfaces/` (CLI, network, UI adapters).
- Lower layers must never depend on higher layers. `core/` should not know that
  `interfaces/` exists.
- Keep headers minimal. Prefer forward declarations over `#include` when only a
  pointer/reference to a type is needed. Keep implementation details out of headers.
- One class/responsibility per file where reasonable. Avoid "god files" that mix
  unrelated concerns.

## 4. Naming & Structure

- Types: `PascalCase`. Functions/variables: `snake_case`. Constants: `kPascalCase`
  or `UPPER_SNAKE_CASE` — pick one and stay consistent within the project.
- Namespaces mirror the folder structure and layer, not arbitrary grouping.
- File names match the primary type/class they define (`Player.hpp` → `class Player`).
- No abbreviations that aren't immediately obvious. Clarity over brevity.

## 5. Interfaces & Abstraction

- Don't introduce an abstract interface (pure virtual class) until there are at
  least two real implementations, or a concrete testing need for one. Speculative
  abstraction adds indirection without payoff.
- Prefer composition over inheritance. Use inheritance only for genuine "is-a"
  polymorphic relationships, not for code reuse.
- Keep interfaces small — a few cohesive methods, not a catch-all.
- Always mark overriding virtual functions with `override`, and `final` where no
  further overriding should happen. Catches signature-mismatch bugs at compile time
  instead of silently creating a new, non-overriding virtual function.

## 6. Testing

- New non-trivial logic should be paired with a unit test in the same PR/change.
- Tests live in a mirrored `tests/` structure next to the code they test.
- Prefer testing behavior through public interfaces, not internal implementation
  details.

## 7. Build & Dependencies

- Don't add a third-party dependency for something the standard library already
  does well.
- Any new dependency must be justified in the commit message or PR description.
- Keep build configuration (CMakeLists, etc.) in sync with actual includes — no
  implicit/transitive include reliance.

## 8. Const-Correctness & Type Idioms

- Mark everything `const` that isn't meant to change — parameters, methods, and
  local variables. It documents intent and lets the compiler catch accidental
  mutation.
- Pass and return small/primitive types (`int`, `bool`, `double`, etc.) by value,
  not by `const &`. A reference to a primitive is a pointer under the hood and is
  typically slower than passing the value directly in a register.
- Avoid boolean parameters in function signatures (`create_widget(true)`) — the
  meaning isn't visible at the call site. Use an enum, or split into two clearly
  named functions instead.
- Prefer standard algorithms (`std::transform`, `std::accumulate`, ranges, etc.)
  over hand-rolled raw loops when one fits the job. A loop that just indexes into
  a container with `[]` is often a sign an algorithm was skipped.
- Mark single-argument constructors and conversion operators `explicit` unless an
  implicit conversion is genuinely intended. Prevents silent, surprising
  conversions the caller didn't ask for.

## 9. General Agent Behavior

- When modifying existing code, match the existing style in that file even if it
  diverges slightly from this guide, rather than mixing conventions within one file.
- Don't refactor unrelated code while completing a task unless asked.
- When a design decision isn't covered here, choose the option that's easiest to
  read and easiest to delete later — avoid clever or over-engineered solutions.