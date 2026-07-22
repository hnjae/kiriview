# Repository Guidelins

KiriView is a C++23 + Rust support library + KDE Kirigami desktop app built by CMake.
Layout: `src/` (app), `tests/cpp/` (C++ tests), `docs/` (docs), `po/` (translations), `flatpak/` (packaging), `devenv/` (devenv).

## Project conventions

- App ID is always `org.hnjae.kiriview`.
- **Translations:** never author `po/*.po` content yourself. Only update templates or existing files with strings supplied by a translator, upstream tooling, or an explicit request.

## External dependency and upstream boundaries

- Do not add, replace, vendor, or patch external libraries unless the user explicitly asks for that change.
- Do not modify upstream, third-party, generated, vendored, or packaged dependency sources as a workaround unless explicitly requested.
- If the correct or straightforward fix appears to require adding an external dependency, changing dependency versions, patching upstream code, or carrying a local workaround for an upstream or library defect, stop before implementing and ask the user how they want to proceed.
- When asking, briefly present the tradeoff, including the dependency or upstream option and any local workaround option.
- Do not avoid this pause by implementing a convoluted local workaround when a dependency addition or upstream patch is the clean solution.

## Test ownership

Tests must stay within the ownership boundary of the code under test. Do not assert unrelated repository artifacts such as `justfile`, Flatpak manifests, CI wiring, packaging metadata, or maintenance scripts from runtime/component tests. If those artifacts need policy coverage, add a dedicated lint or check task at the owning layer instead.

## Verification discipline

**Verification is layered: stay focused while iterating, run every affected app ownership group's completion checks before handoff, and reserve the complete app gate for changes that span those groups.**

Do not run the complete `devenv tasks run --mode=single ci:app` workflow by default. Run it only when the user explicitly requests it or the table explicitly requires it. If multiple rows apply, combine their checks but run duplicate task groups only once. Run multiple listed commands sequentially from left to right; do not parallelize Cargo-backed completion checks because native lint preparation refreshes shared Cargo artifacts.

| Situation                                                                                                                            | What to run                                                                                                                                                                                                                                                    |
| ------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Iterating on code                                                                                                                    | Smallest relevant focused checks: targeted unit/integration tests, `devenv tasks run --mode single ci:app:lint:qml` for touched QML, clippy/check on touched Rust crates, scoped C++/Qt lint.                                                                  |
| Intent-only commit (spec / architecture / expected-failing regression test before implementation)                                    | Run only the relevant documentation check or focused behavior test; this intermediate commit is not a handoff and does not trigger completion rows unless the change is unusually risky.                                                                       |
| Finishing a code change                                                                                                              | Run every applicable completion row below; do not add unrelated language or packaging groups.                                                                                                                                                                  |
| Touched Rust-owned implementation or Rust tests, without changing cross-language wiring                                              | Prefer a filtered host Rust test while iterating; before handoff, run `devenv tasks run --mode single ci:app:test:rust` and `devenv tasks run --mode single ci:app:lint:rust`.                                                                                 |
| Touched C++ source/header-owned implementation, excluding QML-only and cross-language wiring changes                                 | Prefer a focused CTest target while iterating; before handoff, run `devenv tasks run --mode single ci:app:test:cpp` and `devenv tasks run --mode single ci:app:lint:cpp`. Run `just build-with-tests` when Flatpak-only decoder or codec behavior is in scope. |
| Touched only C++ tests, test support, or CMake test registration                                                                     | Prefer a focused CTest target while iterating; before handoff, run `devenv tasks run --mode single ci:app:test:cpp`. `ci:app:lint:cpp` does not lint `tests/cpp/`; add it only when production C++ sources or headers also changed.                            |
| Touched QML files or QML structure                                                                                                   | Run `devenv tasks run --mode single ci:app:lint:qml` plus the relevant Qt/QML CTest target; use `devenv tasks run --mode single ci:app:test:cpp` if no focused CTest target covers the behavior.                                                               |
| Changed a behavior contract spanning Rust and C++/Qt without changing build or FFI wiring                                            | Run `devenv tasks run ci:app:lint:cpp`; intentionally omit `--mode single` so its declared dependency chain runs Rust tests, C++ tests, Rust lint, and C++ lint in order.                                                                                      |
| Touched i18n source strings, generated catalog templates, or generated catalogs                                                      | Run `devenv tasks run --mode single ci:app:i18n:pot-check`; also run `devenv tasks run --mode single ci:app:i18n:check` when `po/*.po` files changed.                                                                                                          |
| Touched app lint rules or lint configuration                                                                                         | Run the affected lint task with `--mode single`; use `devenv tasks run --mode single ci:app:lint` when the affected app lint surface is broad.                                                                                                                 |
| Touched `org.hnjae.kiriview.desktop`                                                                                                 | Run `devenv tasks run --mode single ci:app:lint:desktop`.                                                                                                                                                                                                      |
| Changed app-local cross-language build or FFI wiring, native source manifests, or app-local host dependency wiring                   | Run `devenv tasks run --mode single ci:app`.                                                                                                                                                                                                                   |
| Touched Flatpak finish-args or permission policy without changing build/runtime behavior                                             | Run `devenv tasks run --mode single ci:app:lint:flatpak`.                                                                                                                                                                                                      |
| Touched Flatpak packaging, runtime dependencies, bundled patches/codecs, or manifest build/test behavior                             | Run `devenv tasks run --mode single ci:app:lint:flatpak` when `org.hnjae.kiriview.json` changed, then run `just build-with-tests`.                                                                                                                             |

Always report skipped focused or completion checks.

`devenv tasks run` accepts app ownership prefix groups such as `ci:app:test`, `ci:app:lint`, and `ci:app`. Use `--mode single` with prefix groups to run only tasks matching that app prefix and prevent dependency resolution from expanding the verification scope. Omit `--mode single` only when intentionally requesting a task's declared dependency chain, such as `devenv tasks run ci:app:lint:cpp`.

## Commands

- Run direct `devenv` commands from the repository root; app `just` recipes select the root environment automatically.
- `devenv shell` — development environment when invoked from the repository root.
- `just build` / `just build-with-tests` — Flatpak build in `build-dir/` (tests off / on).
- `just run` — launch from `build-dir/`.
- All app lint and app-owned packaging/policy tasks: `devenv tasks run --mode single ci:app:lint`
- Lint Rust / C++ / QML only: `devenv tasks run --mode single ci:app:lint:rust`, `devenv tasks run --mode single ci:app:lint:cpp`, `devenv tasks run --mode single ci:app:lint:qml`
- Run one app C++ analyzer against the current compilation database: `devenv tasks run --mode single ci:app:lint:cpp:clang-tidy` or `devenv tasks run --mode single ci:app:lint:cpp:clazy`; first run `devenv tasks run --mode single ci:app:lint:cpp:prepare` when the database may be stale.
- App Rust and C++ test groups: `devenv tasks run --mode single ci:app:test`
- Individual Rust / C++ test group: `devenv tasks run --mode single ci:app:test:rust`, `devenv tasks run --mode single ci:app:test:cpp`

## Targeted test recipes

Enter `devenv shell` at the repository root, then `cd app`; the snippets below assume that active shell and app working directory.

**Rust (prefer a filtered lib test before the full suite)** — `<filter>` e.g. `imagezoomstate`:

```sh
jobs="${KIRIVIEW_JOBS:-$(( $(nproc) / 2 + 1 ))}"
env CARGO_TARGET_DIR=target cargo nextest run --locked --lib --all-features --build-jobs "$jobs" --test-threads "$jobs" <filter>
```

**C++/Qt (configure the application-owned CMake graph, then run the matching CTest target)** — `<test_target>` e.g. `tst_imagezoomstate`:

For the full host C++ test task, run `devenv tasks run --mode single ci:app:test:cpp` when you intentionally want only the C++ task; it skips declared Rust test dependencies while CMake still builds the required Rust support library and generated CXX bridges. Without `--mode single`, `ci:app:test:cpp` runs `ci:app:test:rust` first. The CMake build compiles C++ test binaries against the same application libraries used by the executable.

```sh
jobs="${KIRIVIEW_JOBS:-$(( $(nproc) / 2 + 1 ))}"
cmake -S . -B target/devenv/cmake -DCMAKE_BUILD_TYPE=Debug -DKIRIVIEW_BUILD_TESTS=ON
cmake --build target/devenv/cmake --target <test_target> --parallel "$jobs"
env LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 ctest --test-dir target/devenv/cmake -R '^<test_target>$' --output-on-failure --parallel "$jobs"
```
