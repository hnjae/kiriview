# Application Guidelines

KiriView is a KDE Kirigami desktop app with a Rust support library integrated into its CMake build.
Layout: `src/` (app), `tests/cpp/` (C++ tests), `docs/` (docs), `po/` (translations), `flatpak/` (packaging), `devenv/` (devenv).

## Project conventions

- App ID is always `org.hnjae.kiriview`.
- **Translations:** never author `po/*.po` content yourself. Only update templates or existing files with strings supplied by a translator, upstream tooling, or an explicit request.

## Verification discipline

**Verification is layered: stay focused while iterating, run every affected app ownership group's completion checks before handoff, and reserve the complete app gate for changes that span those groups.**

Do not run the complete `devenv tasks run --mode=single ci:app` workflow by default. Run it only when the user explicitly requests it or the table explicitly requires it. If multiple rows apply, combine their checks but run duplicate task groups only once. Run multiple listed commands sequentially from left to right. Do not parallelize tasks within the Rust chain because they share `target/`, or within the CMake chain because they share `target/devenv/cmake`; the complete app gate may schedule the independent Rust and CMake chains separately.

| Situation                                                                                                          | What to run                                                                                                                                                                                                                                                           |
| ------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Iterating on code                                                                                                  | Smallest relevant focused checks: targeted unit/integration tests, `devenv tasks run --mode=single ci:app:lint:qml` for touched QML, clippy/check on touched Rust crates, scoped C++/Qt lint.                                                                         |
| Intent-only commit (spec / architecture / expected-failing regression test before implementation)                  | Run only the relevant documentation check or focused behavior test; this intermediate commit is not a handoff and does not trigger completion rows unless the change is unusually risky.                                                                              |
| Finishing a code change                                                                                            | Run every applicable completion row below; do not add unrelated language or packaging groups.                                                                                                                                                                         |
| Touched Rust-owned implementation or Rust tests, without changing cross-language wiring                            | Prefer a filtered host Rust test while iterating; before handoff, run `devenv tasks run --mode=single ci:app:test:rust` and `devenv tasks run --mode=single ci:app:lint:rust`.                                                                                        |
| Touched C++ source/header-owned implementation, excluding QML-only and cross-language wiring changes               | Prefer a focused CTest target while iterating; before handoff, run `devenv tasks run --mode=single ci:app:test:cpp` and `devenv tasks run --mode=single ci:app:lint:cpp`. Run `just build-flatpak-with-test` when Flatpak-only decoder or codec behavior is in scope. |
| Touched only C++ tests, test support, or CMake test registration                                                   | Prefer a focused CTest target while iterating; before handoff, run `devenv tasks run --mode=single ci:app:test:cpp`. `ci:app:lint:cpp` does not lint `tests/cpp/`; add it only when production C++ sources or headers also changed.                                   |
| Touched QML files or QML structure                                                                                 | Run `devenv tasks run --mode=single ci:app:lint:qml` plus the relevant Qt/QML CTest target; use `devenv tasks run --mode=single ci:app:test:cpp` if no focused CTest target covers the behavior.                                                                      |
| Changed a behavior contract spanning Rust and C++/Qt without changing build or FFI wiring                          | Run `devenv tasks run --mode=single ci:app` so the independent Rust and CMake verification chains both run.                                                                                                                                                           |
| Touched i18n source strings, generated catalog templates, or generated catalogs                                    | Run `devenv tasks run --mode=single ci:app:i18n:pot-check`; also run `devenv tasks run --mode=single ci:app:i18n:check` when `po/*.po` files changed.                                                                                                                 |
| Touched app lint rules or lint configuration                                                                       | Run the affected lint task with `--mode=single`; use `devenv tasks run --mode=single ci:app:lint` when the affected app lint surface is broad.                                                                                                                        |
| Touched `org.hnjae.kiriview.desktop`                                                                               | Run `devenv tasks run --mode=single ci:app:lint:desktop`.                                                                                                                                                                                                             |
| Changed app-local cross-language build or FFI wiring, native source manifests, or app-local host dependency wiring | Run `devenv tasks run --mode=single ci:app`.                                                                                                                                                                                                                          |
| Touched Flatpak finish-args or permission policy without changing build/runtime behavior                           | Run `devenv tasks run --mode=single ci:app:lint:flatpak`.                                                                                                                                                                                                             |
| Touched Flatpak packaging, runtime dependencies, bundled patches/codecs, or manifest build/test behavior           | Run `devenv tasks run --mode=single ci:app:lint:flatpak` when `org.hnjae.kiriview.json` changed, then run `just build-flatpak-with-test`.                                                                                                                             |

Always report skipped focused or completion checks.

`devenv tasks run` accepts app ownership prefix groups such as `ci:app:test`, `ci:app:lint`, and `ci:app`. Use `--mode=single` with prefix groups to run only tasks matching that app prefix and prevent dependency resolution from expanding the verification scope. Omit `--mode=single` only when intentionally requesting one ownership chain's declared dependencies, such as `devenv tasks run ci:app:lint:cpp` for the CMake chain.

## Commands

- `just build` — configure and build the native application with CMake and Ninja in `build-ninja/`.
- All app lint and app-owned packaging/policy tasks: `devenv tasks run --mode=single ci:app:lint`
- Lint Rust / C++ / QML only: `devenv tasks run --mode=single ci:app:lint:rust`, `devenv tasks run --mode=single ci:app:lint:cpp`, `devenv tasks run --mode=single ci:app:lint:qml`
- Run one app C++ analyzer against the current compilation database: `devenv tasks run --mode=single ci:app:lint:cpp:clang-tidy` or `devenv tasks run --mode=single ci:app:lint:cpp:clazy`; first run `devenv tasks run --mode=single ci:app:lint:cpp:prepare` when the database may be stale.
- App Rust and C++ test groups: `devenv tasks run --mode=single ci:app:test`
- Individual Rust / C++ test group: `devenv tasks run --mode=single ci:app:test:rust`, `devenv tasks run --mode=single ci:app:test:cpp`

## Targeted test recipes

Enter `devenv shell` at the repository root, then `cd app`; the snippets below assume that active shell and app working directory.

**Rust (prefer a filtered lib test before the full suite)** — `<filter>` e.g. `imagezoomstate`:

```sh
jobs="${KIRIVIEW_JOBS:-$(( $(nproc) / 2 + 1 ))}"
env CARGO_TARGET_DIR=target cargo nextest run --locked --lib --all-features --build-jobs "$jobs" --test-threads "$jobs" <filter>
```

**C++/Qt (configure the application-owned CMake graph, then run the matching CTest target)** — `<test_target>` e.g. `tst_imagezoomstate`:

For the full host C++ test task, run `devenv tasks run --mode=single ci:app:test:cpp`; CMake builds the required Rust support library and generated CXX bridges without running the independent Rust test task. Use `devenv tasks run --mode=single ci:app:test` when both Rust and C++ test groups are required. The CMake build compiles C++ test binaries against the same application libraries used by the executable.

```sh
jobs="${KIRIVIEW_JOBS:-$(( $(nproc) / 2 + 1 ))}"
cmake -S . -B target/devenv/cmake -DCMAKE_BUILD_TYPE=Debug -DKIRIVIEW_BUILD_TESTS=ON
cmake --build target/devenv/cmake --target <test_target> --parallel "$jobs"
env LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 ctest --test-dir target/devenv/cmake -R '^<test_target>$' --output-on-failure --parallel "$jobs"
```
