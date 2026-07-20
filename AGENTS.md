# Repository Guidelines

## Commands

`just format`
: Format the complete repository with the root treefmt configuration.

`just format-check`
: Verify repository formatting without changing files.

`just test`
: Run all repository test tasks.

`just lint`
: Run all repository lint and policy tasks.

`just check`
: Run the complete repository integration gate.

## Repository integration gate

Component-local changes use the completion checks defined by that component's `AGENTS.md`. Run the complete repository gate with `devenv tasks run --mode single ci` only when root fan-in or check-task orchestration changes, shared/root devenv wiring changes, files across multiple component ownership groups change, or the user explicitly requests a merge/release integration gate and no trusted full-CI result covers the exact commit.

`devenv tasks run` accepts ownership prefix groups such as `ci`, `ci:test`, and `ci:lint`. Use `--mode single` with prefix groups to run only tasks matching that prefix and prevent dependency resolution from expanding the verification scope. The `ci` prefix selects every repository CI task. The `ci:test` and `ci:lint` tasks are root fan-in points whose owning component endpoints register themselves with `before`.

Always report any trusted external full-CI result used in place of a required repository integration gate.

## Project conventions

- 4-space indentation for Rust, QML, C++, and headers.
- **Licensing**: AGPL-3.0-or-later with REUSE checks. New source files need SPDX copyright + license headers; cover generated/metadata files in `REUSE.toml` when inline headers are impractical.

## Policies

- **Pre-release: no backward compatibility.** Do not preserve compatibility for configs, APIs, or internal formats unless explicitly asked.

## Documentation

Apply the following routing within each component that has a `docs/` directory:

- `docs/spec/` contains user-visible behavior and public API contracts, including visible unsupported states and disabled controls. Specs describe what callers can observe and rely on; they must not include implementation strategy, internal type names, milestone labels, test strategy, test IDs, coverage manifests, CI jobs, or release-gate mechanics.
- `docs/spec/README.md` is an index only. Keep each spec as a per-subject file instead of aggregating all behavior into one document.
- `docs/architecture/` is the normative source for durable design intent: subsystem boundaries, ownership rules, data flow, major tradeoffs, and constraints that should survive implementation detail changes. Add ADRs when dated rationale or tradeoffs need preservation. Do not use architecture docs for milestone acceptance criteria, deferred-work lists, exhaustive test inventories, or CI policy.
- `docs/research/` contains investigation notes and source findings. Research informs design but is not normative unless promoted into `docs/spec/` or `docs/architecture/`.
