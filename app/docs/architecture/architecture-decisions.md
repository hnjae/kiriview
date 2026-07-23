# Architecture Decisions

Use `docs/adr/NNNN-title.md` to preserve dated rationale and consequences for durable decisions whose context would otherwise be lost. Keep ADRs short:

- Context
- Decision
- Consequences

ADR-worthy decisions include changing a language or build ownership boundary, replacing a media decoding or rendering family, moving workflow state ownership across a component boundary, or choosing a durable dependency boundary whose rationale would otherwise be lost.

Architecture contract files remain the normative source for the intended end-state design. They capture the durable boundary or invariant resulting from an ADR without repeating supplier inventories, discarded alternatives, or implementation mechanics that are relevant only to the decision record. ADRs preserve rationale and consequences; they must not override the architecture contracts or be used as deferred-work lists or implementation-status indexes.
