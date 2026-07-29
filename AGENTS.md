# Repository Guidelines

## Commands

Run all `devenv` commands, including `devenv tasks run` commands documented in component `AGENTS.md` files, from the repository root.

Run repository `just` recipes from the repository root and component `just` recipes from that component's directory. Component recipes enter the repository-managed `devenv` automatically.

`just format`
: Format the complete repository with the root treefmt configuration.

`just format-check`
: Verify repository formatting without changing files.

`just test`
: Run all repository test tasks.

`just lint`
: Run all repository lint and policy tasks.

`just build`
: Build the Flatpak application in `app/build-dir/`.

`just build-flatpak-with-test`
: Build the Flatpak application with manifest tests enabled.

`just run`
: Launch KiriView from the Flatpak build in `app/build-dir/`.

`devenv tasks run --mode=single ci:repo:lint:cmake`
: Lint all tracked `CMakeLists.txt`, `*.cmake`, and `*.cmake.in` files.

`just check`
: Run the complete repository integration gate.

## Repository integration gate

### Scheduling

Broad gates—the component completion groups and repository integration gate—validate a handoff candidate. An internal slice or green commit does not by itself trigger them. While iterating, run the smallest relevant focused tests and lints. When a material boundary between ownership groups changes, run the narrowest focused integration check that exercises that boundary as soon as the boundary is coherent. When the intended implementation and cleanup are complete, commit the candidate and obtain a passing result for every broad gate required by the applicability rules below and component instructions, against that exact commit. Do not run a broad gate merely because an internal slice ended.

A required broader gate satisfies every narrower completion group that it actually executes against the same exact commit; do not run a subsumed group separately. Run only required checks that the broader gate does not cover, such as an artifact build outside the repository CI task graph. A trusted external result may substitute for the repository integration gate only as allowed below.

If a required broad gate fails or any tracked content changes after it passes, use focused checks to diagnose and verify the fix, commit the new candidate, and rerun every still-required broad gate against the new exact commit. Run a broad gate before the handoff-candidate stage only when focused checks cannot provide adequate signal for a material cross-component integration risk and its result is needed to choose or safely continue implementation, or when the user explicitly requests it. This scheduling rule does not defer cheap commit hooks, the expected-failing step of a test-first boundary change, or other focused checks that provide timely signal.

### Applicability

At handoff, documentation-only changes require `just format-check`; skip component code checks and the complete repository gate unless the change is unusually risky or the user explicitly requests broader verification. Documentation-only changes do not require the complete gate solely because they span multiple component documentation trees.

Changes to tracked `CMakeLists.txt`, `*.cmake`, or `*.cmake.in` files require `devenv tasks run --mode=single ci:repo:lint:cmake` in addition to the owning component's completion checks.

Component-local changes use the completion checks defined by that component's `AGENTS.md`. Run the complete repository gate with `devenv tasks run --mode=single ci` only when root fan-in or check-task orchestration changes, shared/root devenv wiring changes, implementation or configuration files across multiple top-level component ownership boundaries change, or the user explicitly requests a merge/release integration gate and no trusted full-CI result covers the exact commit.

`devenv tasks run` accepts ownership prefixes such as `ci` and component-specific task prefixes. Use `--mode=single` with an ownership prefix to run only tasks matching that prefix and prevent dependency resolution from expanding the verification scope. The `ci` prefix selects every repository CI task. Run the exact `ci:test` and `ci:lint` fan-in tasks without `--mode=single` so their owning component endpoints registered with `before` run first.

Always report any trusted external full-CI result used in place of a required repository integration gate.

## Project conventions

- C++ targets use C++23.
- Qt targets require Qt 6.11 or later.
- 4-space indentation for Rust, QML, C++, and headers.
- **Licensing**: AGPL-3.0-or-later with REUSE checks. New source files need SPDX copyright + license headers; cover generated/metadata files in `REUSE.toml` when inline headers are impractical.

## Policies

- **Pre-release: no backward compatibility.** Do not preserve compatibility for configs, APIs, or internal formats unless explicitly asked.

## External dependency and upstream boundaries

- Do not add, replace, vendor, or patch external libraries unless the user explicitly asks for that change.
- Do not modify upstream, third-party, generated, vendored, or packaged dependency sources as a workaround unless explicitly requested.
- If the correct or straightforward fix appears to require adding an external dependency, changing dependency versions, patching upstream code, or carrying a local workaround for an upstream or library defect, stop before implementing and ask the user how they want to proceed.
- When asking, briefly present the tradeoff, including the dependency or upstream option and any local workaround option.
- Do not avoid this pause by implementing a convoluted local workaround when a dependency addition or upstream patch is the clean solution.

## Test ownership

Tests must stay within the ownership boundary of the code under test. Do not assert unrelated repository artifacts such as `justfile`, Flatpak manifests, CI wiring, packaging metadata, or maintenance scripts from runtime or component tests. If those artifacts need policy coverage, add a dedicated lint or check task at the owning layer instead.

## Documentation

Apply the following routing within each component that has a `docs/` directory:

- `docs/spec/` contains user-visible behavior and public API contracts, including visible unsupported states and disabled controls. Specs describe what callers can observe and rely on; they must not include implementation strategy, internal type names, milestone labels, test strategy, test IDs, coverage manifests, CI jobs, or release-gate mechanics.
- `docs/spec/README.md` is an index only. Keep each spec as a per-subject file instead of aggregating all behavior into one document.
- `docs/architecture/` is the normative source for durable design intent: subsystem boundaries, ownership rules, data flow, major tradeoffs, and constraints that should survive implementation detail changes. Do not use architecture docs for milestone acceptance criteria, deferred-work lists, exhaustive test inventories, or CI policy.
- `docs/adr/` contains dated rationale, alternatives, and consequences for durable decisions. ADRs preserve decision context but do not replace or override normative architecture contracts.
- `docs/research/` contains investigation notes and source findings. Research informs design but is not normative unless promoted into `docs/spec/` or `docs/architecture/`.
