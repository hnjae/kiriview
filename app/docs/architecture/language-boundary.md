# Language Boundary

The Rust/C++ boundary is a narrow media-support capability boundary, not a product-policy boundary. The C++ application owns product policy, workflows, state transitions, cache decisions, navigation, format routing, Qt/KDE integration, and all public state.

This document owns the canonical Rust support dependency allowlist. The statically linked Rust support library owns only capability implementations backed by these required Rust libraries:

- `nom-exif` for image and direct-video embedded metadata parsing.
- `png` for streaming APNG byte decoding and raw subframe metadata.
- `resvg` for self-contained static SVG parsing and rasterization. QtSvg does not implement the complete SVG behavior required by KiriView and is not a substitute for this boundary.
- `xdg-thumbnail` for desktop thumbnail-cache lookup and installation.

Other architecture contracts refer to these named capabilities instead of restating or extending the dependency allowlist. A capability-specific contract may still name its required library when that choice explains a durable constraint, as with `resvg` and SVG behavior.

Rust may contain validation, byte conversion, opaque decoder state, and other adapter logic needed to expose those capabilities safely. That supporting code must remain cohesive with one of the named capabilities and must not become a general home for Qt-independent application logic.

C++ owns both Qt-facing code and plain product policy. Pure policy uses ordinary value types, explicit snapshots, typed plans, and deterministic functions instead of acquiring QObject identity or signal-driven state solely because it is written in C++. Native ownership, borrowing, buffer, and callback code follows [C++ And Qt Safety](cpp-qt-safety.md).

Rust must not call back into Qt/KDE adapters, own application source identity, schedule application work, publish UI state, or execute product workflow effects. C++ passes plain bytes and values into the support library, converts returned payloads into Qt-owned values, and retains all application lifecycle and stale-completion authority.
