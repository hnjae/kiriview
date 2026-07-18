# Source Key Contract

The C++ location and session domains own source identity keys for route sealing, route equality, route freshness, session snapshots, and direct-media scope identity.

Top-level route keys normalize `QUrl` path segments, preserve query and fragment components, and compare fully encoded identity strings. They do not case-fold and do not resolve symlinks, aliases, shortcuts, or platform filesystem equivalences.

Local file keys additionally clean local path syntax and make relative local paths absolute for identity only. The public `sourceUrl` remains the requested URL rather than the identity URL.

Direct-media scope identity is `{ current key, parent key, generation }`. The current and parent keys use the navigation URL and parent location sealed into the active navigation-source snapshot before applying source-key normalization. Platform probes for document-portal host paths and platform archive-entry restoration are source-entry adapter work performed once per accepted source snapshot; source-key construction and equality consume that snapshot and never trigger platform probes. Replacing or discarding the active source snapshot is the invalidation boundary for both positive and negative platform facts.

Direct-media freshness changes only when the effective current key or parent scope key changes. Resolving pending direct-image confirmation to an equivalent displayed URL is a phase change within the same freshness generation.

Archive-entry key families, render keys, thumbnail cache keys, predecode candidate keys, and sandbox-specific freshness beyond the defined navigation-source handling are defined by the extension contracts. Code that crosses adapter, cache, thumbnail, predecode, or render boundaries must use typed family-specific keys instead of a generic URL identity key.

The generic top-level source key is not an adapter boundary type. Extension boundaries must use family-specific key structs whose equality cannot accidentally compare unrelated families. Durable identity and freshness generation are separate types: durable row keys compare reusable identity, while explicitly generation-scoped demand and revision keys compare both durable identity and freshness. A family must not place generation on a durable key while silently omitting it from equality or hashing.
