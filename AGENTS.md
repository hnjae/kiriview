# Agent Guidelines

## Documentation

- `docs/spec/` contains user-facing behavior and public API contracts. Specs describe what callers can observe and rely on; they must not include implementation strategy, internal type names, test IDs, coverage manifests, CI jobs, or release-gate mechanics.
- `docs/spec/README.md` is an index only. Keep each spec as a per-subject file instead of aggregating all behavior into one document.
- `docs/architecture/` contains durable design intent: subsystem boundaries, ownership rules, data flow, major tradeoffs, and constraints that should survive implementation detail changes. Do not use architecture docs for milestone checklists, exhaustive test inventories, or CI policy.
- `docs/roadmap/` contains intermediate milestones, non-goals, sequencing, and acceptance summaries. Milestone docs should stay human-readable and should not introduce machine-enforced coverage IDs until the project actually needs release-gate automation.
- `docs/research/` contains investigation notes and source findings. Research informs design but is not normative unless promoted into `docs/spec/` or `docs/architecture/`.
- Prose is concise and technical. Do not hard-wrap paragraphs: use one paragraph or list item per source line unless Markdown syntax requires otherwise.
- Use Mermaid for diagrams when a relationship or state transition is easier to understand visually.

## Change Discipline

- For user-visible behavior changes, update the relevant spec before implementation.
- For structural changes, update the relevant architecture document before implementation.
- For milestone scope changes, update `docs/roadmap/` rather than adding milestone language to specs or architecture.
- Pure repository maintenance such as build wiring, formatting, linting, generated metadata, or dependency updates does not require an intent document first.

## Git Commits

- Use Conventional Commits with a scope whenever one clearly applies.
- Commit on task completion unless the user has said otherwise.
- When Codex materially authors a commit's changes, add this trailer in the commit message footer after a blank line: `Co-authored-by: Codex <noreply@openai.com>`.
