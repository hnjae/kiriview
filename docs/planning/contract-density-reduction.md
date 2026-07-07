# Architecture Contract Density Reduction Plan

Its goal is to reduce the density of durable architecture contracts without weakening the real ownership, stale-rejection, cache-identity, or language-boundary guarantees that protect KiriView.

## Goals

- Make architecture documents easier to read by separating hard contracts from rationale, examples, and implementation guidance.
- Preserve the end-state meaning of `docs/spec/` and `docs/architecture/`.
- Remove duplicated architecture rules that are repeated across multiple files with slightly different wording.
- Replace overly specific prohibition lists with smaller invariant-based contracts where tests already enforce the details.
- Keep behavior specs focused on user-visible behavior only.
- Keep architecture docs focused on durable ownership, boundary, identity, lifecycle, and testing contracts.

## Non-Goals

- Do not change product behavior.
- Do not weaken ownership, stale-completion rejection, cache identity, provider boundary, QML facade, or Rust/C++ language-boundary rules.
- Do not move milestone plans, deferred work, acceptance criteria, or implementation progress into `docs/spec/` or `docs/architecture/`.
- Do not rewrite all documentation for style alone.
- Do not remove a contract unless its replacement remains testable or clearly enforced by type/API structure.

## End-State Documentation Rule

`docs/spec/` and `docs/architecture/` must describe the intended final state after each cleanup commit.

Planning notes, audit findings, sequencing, and temporary migration details belong under `docs/planning/`.

When editing architecture documents, write them as if the cleaned-up structure already exists. Do not describe “before”, “after”, “phase”, “currently”, or “later” unless the file is under `docs/planning/`.

## Milestone 1: Contract Density Audit

Scope: Audit `docs/architecture/` for paragraphs that mix hard contracts, rationale, examples, test strategy, and implementation history.

Classify each dense rule as one of:

- `Must`: breaking this creates an architecture bug.
- `Should`: preferred structure, but not a hard invariant.
- `Rationale`: explanation for why the rule exists.
- `Example`: illustrative, not exhaustive.
- `Test`: verification ownership or regression coverage.
- `Planning`: milestone, deferred work, sequencing, or acceptance language.

Acceptance:

- Produce a planning note listing the highest-density files and the type of cleanup each needs.
- Identify duplicated or near-duplicated rules across architecture files.
- Identify any spec text that accidentally describes implementation details.
- No `docs/spec/` or `docs/architecture/` changes are required in this milestone.

Verification: Markdown review and `git diff --check`.

## Milestone 2: Normalize Architecture Document Shape

Scope: Introduce a consistent document shape for dense architecture files.

Preferred shape:

- `Contract`: hard end-state rules.
- `Ownership`: canonical owner and forbidden secondary owners.
- `Identity`: source, revision, generation, cache-key, or stale-rejection identity.
- `Lifecycle`: creation, invalidation, completion, retention, and release rules.
- `Testing`: boundary tests that protect the contract.
- `Rationale`: short explanation only where the contract would otherwise be surprising.

Acceptance:

- Update only architecture files whose density causes ambiguity.
- Do not add milestone sequencing or temporary migration text.
- Do not expand content while reorganizing; the net result should be shorter or clearer.

Verification: `git diff --check`; full CI not required for docs-only cleanup.

## Milestone 3: Deduplicate Cross-Document Contracts

Scope: Reduce repeated rules across `state-ownership.md`, `workflow-shape.md`, `provider-rendering.md`, `thumbnail-source-adapters.md`, `extension-contracts.md`, and `testing-strategy.md`.

Rules:

- One file should own the canonical contract.
- Other files should reference that contract briefly instead of restating it.
- If two documents need the same concept, one should define it and the other should describe only its local consequence.

Likely candidates:

- Provider request boundaries.
- Candidate snapshot identity tokens.
- Thumbnail demand window rules.
- Display-source stale-completion rejection.
- QML/facade ownership prohibitions.
- Extension contract test expectations.

Acceptance:

- No architecture rule is duplicated with divergent wording.
- Each major invariant has one obvious canonical home.
- References are short and do not become another full copy of the rule.

Verification: `git diff --check`; docs review.

## Milestone 4: Convert Prohibition Lists Into Invariants

Scope: Replace long “must not” lists with smaller invariant-based contracts where possible.

Example direction:

- Instead of listing many forbidden QML behaviors, define the canonical QML boundary and name the few classes of forbidden ownership bypass.
- Instead of listing every stale-result case in multiple places, define the identity required for acceptance and where each generation/revision is owned.
- Instead of listing every cache misuse, define the cache-key identity and owner lease rule.

Acceptance:

- The resulting contract is shorter but not weaker.
- Concrete forbidden examples may remain only when they prevent a likely regression.
- Boundary tests still have enough specificity to be useful.

Verification: `git diff --check`; run affected documentation lint/checks if available.

## Milestone 5: Move Guidance Out Of Durable Architecture

Scope: Move non-contract guidance from `docs/architecture/` to `docs/planning/` or remove it when obsolete.

Move or remove:

- Milestone acceptance language.
- Deferred-work lists.
- Temporary implementation rationale.
- “During migration” wording.
- Test implementation details that do not define ownership.
- Performance optimization notes that are not durable contracts.

Acceptance:

- `docs/architecture/` reads as an end-state architecture, not a project-management artifact.
- Useful non-contract guidance is preserved under `docs/planning/`.
- Obsolete guidance is deleted.

Verification: `git diff --check`.

## Milestone 6: Spec Boundary Cleanup

Scope: Review `docs/spec/` for implementation details introduced while documenting unsupported states, disabled controls, thumbnails, navigation, or display behavior.

Acceptance:

- Specs describe external behavior only.
- Specs do not mention provider internals, cache internals, snapshot storage, revision tokens, architecture tests, or milestone progress.
- Visible unsupported states and disabled controls remain documented.

Verification: `git diff --check`; no full CI required unless tooling demands it.

## Milestone 7: Type/API Enforcement Follow-Up

Scope: Identify architecture contracts that can be enforced by types or narrow APIs instead of prose.

Candidates:

- Display scope identity versus source identity.
- Candidate-list source identity versus candidate-list revision.
- Direct-media scope generation versus thumbnail navigation generation.
- Provider-ready payloads that must already contain bucket-sized or scope-correct data.

Acceptance:

- Produce a planning artifact listing contracts that should become type/API constraints.
- Do not implement code changes in this milestone unless separately requested.
- This milestone may generate a later implementation plan.

Verification: Planning review only.

## Closeout Criteria

This cleanup is complete when:

- Dense architecture files distinguish hard contracts from rationale and examples.
- Important invariants have one canonical home.
- `docs/spec/` contains user-visible behavior only.
- `docs/architecture/` contains durable end-state contracts only.
- Planning, sequencing, and cleanup notes live under `docs/planning/`.
- No ownership, stale-rejection, cache-identity, provider-boundary, or language-boundary guarantee was weakened.
