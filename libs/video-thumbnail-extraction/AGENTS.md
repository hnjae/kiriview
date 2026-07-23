# Repository Guidelines

## Project Structure & Module Organization

`src/` contains the repository-internal C++23 static library and public headers under `src/VideoThumbnailExtraction/`. Future private headers use the `*_p.h` suffix.

## Build, Test, and Development Commands

Run commands from this component directory; recipes enter the repository-managed `devenv` automatically.

`just configure`
: generate `build-ninja/` with CMake and Ninja.

`just build`
: configure and compile the library.

`just test`
: run the component's complete CI test task.

`just lint`
: run component lint and policy checks.

`just clean`
: remove the generated `build-ninja/` directory.

> [!NOTE]
> From the repository root, run one C++ analyzer against the current compilation database with `devenv tasks run --mode single ci:video-thumbnail-extraction:lint:clang-tidy` or `devenv tasks run --mode single ci:video-thumbnail-extraction:lint:clazy`; first run `devenv tasks run --mode single ci:video-thumbnail-extraction:lint:prepare` when the database may be stale.
