# Repository Guidelines

## Commands

`just setup`
: Materialize root devenv-managed files and install the repository Git hooks.

`just format`
: Format the complete repository with the root treefmt configuration.

`just format-check`
: Verify repository formatting without changing files.

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
