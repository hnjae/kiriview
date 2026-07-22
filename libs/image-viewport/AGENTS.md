# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the repository-internal C++23 library, private headers (`*_p.h`), and public headers under `src/ImageViewport/`. `tests/` contains Qt Test executables, shared support, header-isolation checks, and the test-only QML registration.

## Build, Test, and Development Commands

Run commands from this component directory; recipes enter the repository-managed `devenv` automatically.

- `just configure`: generate `build-ninja/` with CMake and Ninja.
- `just build`: configure and compile the library and tests.
- `just test`: run the component's complete CI test task.
- `just lint`: run component lint and policy checks.
- From the repository root, run one C++ analyzer against the current compilation database with `devenv tasks run --mode single ci:image-viewport:lint:clang-tidy` or `devenv tasks run --mode single ci:image-viewport:lint:clazy`; first run `devenv tasks run --mode single ci:image-viewport:lint:prepare` when the database may be stale.
- `just clean`: remove the generated `build-ninja/` directory.
- From the repository root, `just format` formats the whole tree and `just ci` runs the full integration gate.
