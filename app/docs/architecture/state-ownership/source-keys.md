# Source Key Contract

The location and session domains own source identity keys for route sealing, route equality, route freshness, session snapshots, and direct-media scope identity.

Top-level route keys normalize `QUrl` path segments, preserve query and fragment components, and compare fully encoded identity strings. They do not case-fold and do not resolve symlinks, aliases, shortcuts, or platform filesystem equivalences.

Local file keys additionally clean local path syntax and make relative local paths absolute for identity only. The public source URL projection remains the requested URL rather than the identity URL.

Direct-media scope identity contains the normalized current and parent identities plus owner-issued freshness evidence. The current and parent identities use the navigation URL and parent location sealed into the active navigation-source snapshot before applying source-key normalization. Only an authenticated platform restoration fact, such as an exact document-portal host-path or archive-service mapping, may replace the requested URL when those identities are formed; ordinary or unverifiable file metadata cannot grant a different scope identity. Platform probes for document-portal host paths and platform archive-entry restoration are source-entry adapter work performed once per accepted source snapshot; source-key construction and equality consume that snapshot and never trigger platform probes. Replacing or discarding the active source snapshot is the invalidation boundary for both positive and negative platform facts.

Direct-media freshness changes only when the effective current key or parent scope key changes. Resolving pending direct-image confirmation to an equivalent displayed URL is a phase change under the same freshness evidence.

Archive-entry identities, provider-work identities, thumbnail cache identities, predecode candidate identities, and sandbox-specific freshness beyond the defined navigation-source handling are defined by the extension contracts. Boundaries must preserve the identity family explicitly so unrelated domains cannot compare equal merely because they contain the same URL.

Identity families may use distinct types, tagged values, or another unambiguous representation. Durable identity and freshness remain separate semantic inputs to equality and acceptance: reusable identity comparisons must not accidentally include transient freshness, and lifecycle-scoped comparisons must not silently omit it. Hashing and serialization, where used, must follow the same equality semantics.
