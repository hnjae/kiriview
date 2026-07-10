# Repository Guidelines

KiriView is a Rust + CXX-Qt + KDE Kirigami desktop app.
Layout: `src/` (app), `tests/cpp/` (C++ tests), `docs/` (docs), `po/` (translations), `flatpak/` (packaging), `devenv/` (devenv).

## Project conventions

- 4-space indentation for Rust, QML, C++, and headers.
- App ID is always `org.hnjae.kiriview`.
- **Translations:** never author `po/*.po` content yourself. Only update templates or existing files with strings supplied by a translator, upstream tooling, or an explicit request.
- **Licensing:** AGPL-3.0-or-later with REUSE checks. New source files need SPDX copyright + license headers; cover generated/metadata files in `REUSE.toml` when inline headers are impractical.

## Policies

- **Documentation routing:** `docs/spec/` is for user-visible behavior only, including visible unsupported states and disabled controls; do not put implementation details, milestone labels, test strategy, or coverage IDs there. `docs/architecture/` (start at `README.md`) is the normative source for durable design boundaries and ownership rules, including provider, render, state, language-boundary, FFI, workflow, and architecture test ownership; record those contracts there, add ADRs when their dated rationale or tradeoffs need preservation, never duplicate the contracts here, and do not put milestone acceptance criteria or deferred-work lists there.
- **Pre-release: no backward compatibility.** Do not preserve compatibility for configs, APIs, or internal formats unless explicitly asked.

## External dependency and upstream boundaries

- Do not add, replace, vendor, or patch external libraries unless the user explicitly asks for that change.
- Do not modify upstream, third-party, generated, vendored, or packaged dependency sources as a workaround unless explicitly requested.
- If the correct or straightforward fix appears to require adding an external dependency, changing dependency versions, patching upstream code, or carrying a local workaround for an upstream or library defect, stop before implementing and ask the user how they want to proceed.
- When asking, briefly present the tradeoff, including the dependency or upstream option and any local workaround option.
- Do not avoid this pause by implementing a convoluted local workaround when a dependency addition or upstream patch is the clean solution.

## Test ownership

Tests must stay within the ownership boundary of the code under test. Do not assert unrelated repository artifacts such as `justfile`, Flatpak manifests, CI wiring, packaging metadata, or maintenance scripts from runtime/component tests. If those artifacts need policy coverage, add a dedicated lint or check task at the owning layer instead.

## Verification discipline

**Verification is layered: stay focused while iterating, run every affected ownership group's completion checks before handoff, and reserve full CI for integration gates.**

Do not run the full `devenv tasks run ci` workflow by default; ordinary code completion is not a full-CI trigger. Run it only when the user explicitly requests it or the table explicitly requires it. If multiple rows apply, combine their checks but run duplicate task groups only once. Run multiple listed commands sequentially from left to right; do not parallelize Cargo-backed completion checks because native lint preparation refreshes shared Cargo artifacts.

| Situation                                                                                                                            | What to run                                                                                                                                                                                                                                            |
| ------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Iterating on code                                                                                                                    | Smallest relevant focused checks: targeted unit/integration tests, `devenv tasks run --mode single ci:lint:qml` for touched QML, clippy/check on touched Rust crates, scoped C++/Qt lint.                                                              |
| Intent-only commit (spec / architecture / expected-failing regression test before implementation)                                    | Run only the relevant documentation check or focused behavior test; this intermediate commit is not a handoff and does not trigger completion rows unless the change is unusually risky.                                                               |
| Finishing a code change                                                                                                              | Run every applicable completion row below; do not add unrelated language or packaging groups.                                                                                                                                                          |
| Touched Rust-owned implementation or Rust tests, without changing cross-language wiring                                              | Prefer a filtered host Rust test while iterating; before handoff, run `devenv tasks run --mode single ci:test:rust` and `devenv tasks run --mode single ci:lint:rust`.                                                                                 |
| Touched C++ source/header-owned implementation, excluding QML-only and cross-language wiring changes                                 | Prefer a focused CTest target while iterating; before handoff, run `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`. Run `just build-with-tests` when Flatpak-only decoder or codec behavior is in scope. |
| Touched only C++ tests, test support, or CMake test registration                                                                     | Prefer a focused CTest target while iterating; before handoff, run `devenv tasks run --mode single ci:test:cpp`. `ci:lint:cpp` does not lint `tests/cpp/`; add it only when production C++ sources or headers also changed.                            |
| Touched QML files or QML structure                                                                                                   | Run `devenv tasks run --mode single ci:lint:qml` plus the relevant Qt/QML CTest target; use `devenv tasks run --mode single ci:test:cpp` if no focused CTest target covers the behavior.                                                               |
| Changed a behavior contract spanning Rust and C++/Qt without changing build or FFI wiring                                            | Run `devenv tasks run ci:lint:cpp`; its declared dependency chain runs Rust tests, C++ tests, Rust lint, and C++ lint in order without selecting unrelated CI tasks.                                                                                   |
| Touched i18n source strings, generated catalog templates, or generated catalogs                                                      | Run `devenv tasks run --mode single ci:i18n:pot-check`; also run `devenv tasks run --mode single ci:i18n:check` when `po/*.po` files changed.                                                                                                          |
| Touched formatter config or formatting rules                                                                                         | Run `devenv shell -- treefmt --ci` plus the relevant hook or lint task for the changed file type.                                                                                                                                                      |
| Touched lint rules or lint configuration                                                                                             | Run the affected lint task with `--mode single`; use `devenv tasks run --mode single ci:lint` when the affected lint surface is broad.                                                                                                                 |
| Touched `org.hnjae.kiriview.desktop`                                                                                                 | Run `devenv tasks run --mode single ci:lint:desktop`.                                                                                                                                                                                                  |
| Changed host cross-language build or FFI wiring, native source manifests, check-task orchestration, or host/devenv dependency wiring | Run `devenv tasks run ci`.                                                                                                                                                                                                                             |
| Touched Flatpak finish-args or permission policy without changing build/runtime behavior                                             | Run `devenv tasks run --mode single ci:lint:flatpak`.                                                                                                                                                                                                  |
| Touched Flatpak packaging, runtime dependencies, bundled patches/codecs, or manifest build/test behavior                             | Run `devenv tasks run --mode single ci:lint:flatpak` when `org.hnjae.kiriview.json` changed, then run `just build-with-tests`.                                                                                                                         |
| The user explicitly requests a merge/release integration gate and no trusted full-CI result covers the exact commit                  | Run `devenv tasks run ci` once.                                                                                                                                                                                                                        |

Always report skipped focused or completion checks and any trusted external full-CI result used in their place.

`devenv tasks run` accepts prefix groups: `ci:test`, `ci:lint`, and `ci` run the matching `ci:*` tasks even though those exact aggregate names may not appear in `devenv tasks list`. Use `--mode single` for affected ownership groups so unrelated upstream dependencies do not expand the verification scope.

## Commands

- `devenv shell` — development environment.
- `just build` / `just build-with-tests` — Flatpak build in `build-dir/` (tests off / on).
- `just run` — launch from `build-dir/`.
- `devenv shell -- treefmt` / `devenv shell -- treefmt --ci` — format / CI format check.
- All lint and repository/packaging policy tasks: `devenv tasks run --mode single ci:lint`
- Lint Rust / C++ / QML only: `devenv tasks run --mode single ci:lint:rust`, `devenv tasks run --mode single ci:lint:cpp`, `devenv tasks run --mode single ci:lint:qml`
- Rust and C++ test groups only: `devenv tasks run --mode single ci:test`
- Individual Rust / C++ test group: `devenv tasks run --mode single ci:test:rust`, `devenv tasks run --mode single ci:test:cpp`

## Targeted test recipes

**Rust (prefer a filtered lib test before the full suite)** — `<filter>` e.g. `imagezoomstate`:

```sh
jobs="${KIRIVIEW_JOBS:-$(( $(nproc) / 2 + 1 ))}"
devenv shell -- env CARGO_TARGET_DIR=target cargo nextest run --locked --lib --all-features --build-jobs "$jobs" --test-threads "$jobs" <filter>
```

**C++/Qt (build the Cargo-owned app staticlib and generated headers, then run the matching CTest target)** — `<test_target>` e.g. `test_imagezoomstate`:

For the full host C++ test task, run `devenv tasks run --mode single ci:test:cpp` when you intentionally want only the C++ task; it skips declared Rust test dependencies and builds the required Cargo-owned KiriView app library artifacts inside the task. Without `--mode single`, `ci:test:cpp` runs `ci:test:rust` first. The CMake build compiles C++ test binaries that link that Cargo-produced app library.

```sh
jobs="${KIRIVIEW_JOBS:-$(( $(nproc) / 2 + 1 ))}"
devenv shell -- env CARGO_TARGET_DIR=target cargo build --locked --lib --all-features --jobs "$jobs"
devenv shell -- cmake -S tests/cpp -B target/devenv/cpp-tests -DCMAKE_BUILD_TYPE=Debug -DKIRIVIEW_CARGO_TARGET_DIR="$PWD/target/debug"
devenv shell -- cmake --build target/devenv/cpp-tests --target <test_target> --parallel "$jobs"
devenv shell -- env LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 ctest --test-dir target/devenv/cpp-tests -R '^<test_target>$' --output-on-failure --parallel "$jobs"
```
