# Image Viewport Library Guidelines

## Project Structure & Module Organization

`src/` contains the repository-internal library, private headers (`*_p.h`), and public headers under `src/ImageViewport/`. `tests/` contains Qt Test executables, shared support, header-isolation checks, and the test-only QML registration.

## Build, Test, and Development Commands

`just configure`
: generate `build-ninja/` with CMake and Ninja.

`just build`
: configure and compile the library and tests.

`just test`
: run the component's complete CI test task.

`just lint`
: run component lint and policy checks.

`devenv tasks run --mode=single ci:image-viewport:lint:prepare`
: prepare the compilation database before running an analyzer when the database may be stale.

`devenv tasks run --mode=single ci:image-viewport:lint:clang-tidy`
: run clang-tidy against the current compilation database.

`devenv tasks run --mode=single ci:image-viewport:lint:clazy`
: run Clazy against the current compilation database.

`just clean`
: remove the generated `build-ninja/` directory.

## Verification discipline

Use the smallest relevant focused test while iterating. Before handoff, run `just test` followed by `just lint` for implementation, header, test, or CMake changes. Always report skipped completion checks.
