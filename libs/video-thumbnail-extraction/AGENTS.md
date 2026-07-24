# Video Thumbnail Extraction Library Guidelines

## Project Structure & Module Organization

`src/` contains the repository-internal static library implementation and public headers under `src/VideoThumbnailExtraction/`. `private/` contains component-private headers, which use the `*_p.h` suffix.

## Build, Test, and Development Commands

`just configure`
: generate `build-ninja/` with CMake and Ninja.

`just build`
: configure and compile the library.

`just test`
: run the component's complete CI test task.

`just lint`
: run component lint and policy checks.

`devenv tasks run --mode=single ci:video-thumbnail-extraction:lint:prepare`
: prepare the compilation database before running an analyzer when the database may be stale.

`devenv tasks run --mode=single ci:video-thumbnail-extraction:lint:clang-tidy`
: run clang-tidy against the current compilation database.

`devenv tasks run --mode=single ci:video-thumbnail-extraction:lint:clazy`
: run Clazy against the current compilation database.

`just clean`
: remove the generated `build-ninja/` directory.

## Verification discipline

Use the smallest relevant focused test while iterating. Before handoff, run `just test` followed by `just lint` for implementation, header, test, or CMake changes. Always report skipped completion checks.
