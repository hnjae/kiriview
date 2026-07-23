# Layer Ownership

QML and Kirigami own declarative UI composition:

- Page structure, menus, toolbars, overlays, and UI-local shortcut attachment points.
- Visual placement of action objects supplied by the C++ facade.
- Bindings to documented QObject properties and invokable methods.
- UI-local state expressly assigned to QML by an owner contract. When that state affects shared behavior, QML reports its current facts through the named owner input instead of applying the behavior itself.

C++ QObject and QQuickItem facade classes own the public QML surface:

- `Q_OBJECT`, `Q_PROPERTY`, `Q_INVOKABLE`, signals, and QML registration.
- Conversion between QML-friendly types and internal controller APIs.
- Thin forwarding to controllers and render items.

Facade classes, including the declared repository-internal `ImageViewport` item surface, are the only QML-facing API boundaries. API visibility does not transfer domain ownership: KiriView QML forwards raw image interaction facts through the image-viewport integration owner and does not invoke component mutations directly. Domain behavior must live in the appropriate runtime, presentation, rendering, or policy owner instead of growing facade-owned workflow state.

C++ application code owns product policy, platform integration, and side effects through named owners:

- `QObject` lifetime, signal delivery, thread affinity, and cancellation.
- `QUrl`, `QImage`, `QAction`, `QQuickItem`, image-provider entries, and other Qt objects.
- OS and desktop-environment observers such as system-memory discovery and power-saver portal state.
- `QAction` identity, KDE action collections, configured shortcut state, and shortcut persistence.
- KIO jobs, KDE settings, dialogs, notifications, file operations, and runtime integration.
- Localized user-facing strings for dialogs, menus, actions, notifications, desktop metadata, and other UI surfaces. Localization integration consumes installed translation catalogs and KDE/Qt language settings while preserving the English fallback contract.
- Supported-media capability modules expose canonical extension, MIME-type, and collection-kind facts consumed by routing, open-dialog filters, desktop-file advertising, adjacent navigation eligibility, thumbnail eligibility, and decoder-route selection. C++ image-format policy owns advertised image extension and MIME-type metadata plus decoder-family selection as defined by [Extension Contracts](extension-contracts.md#decoder-contracts). The supported-media capability boundary publishes canonical direct-video extension and MIME-type facts, and collection-access owners publish collection-kind and entry-playability capabilities. Qt/KDE adapters consume those facts and own localized file-dialog labels; no consumer may maintain a divergent supported-media list or become a second format-policy owner.
- The repository-internal `ImageViewport` component as the single owner of accepted image presentation, target transitions, fit and zoom, pan, rotation and mirroring, spread geometry, visible source geometry, per-role animation playback, render admission, and image scene graph resources.
- The image-document integration owner as the application adapter for selected source identity, page pairing, reading and scan policy, component target and command submission, generation correlation, failure-reference resolution, and KiriView-facing projection.
- Image provider-resource owners as owners of source access, decoded payloads, application cache and display-store entries, predecode and refinement work, typed failure records, provider sessions, frame-handle leases, and failure-handle release callbacks only.
- Image render-context adaptation through the component item and its private rendering implementation. The component owns viewport payload admission, backend caps, allocation state, and per-role display budgets and publishes the applicable facts through provider demand. KiriView enforces application cache, display-store, and source-work budgets behind the provider boundary but must not own image textures, scene graph nodes, render commits, or image-load acknowledgements.

C++ policy code owns application decisions independently of whether they require Qt:

- State transitions, workflow plans, and reducer-like decisions.
- Navigation, page pairing, scan and nearest-point planning, deletion-target, follow-up, cache decisions, format inspection, and decoder routing when they are computed from plain value snapshots.
- Pure calculations where the same input produces the same output.

The Rust support static library owns the implementations behind the canonical [Language Boundary](language-boundary.md) allowlist: embedded metadata parsing, APNG stream decoding, static SVG parsing and rasterization, and desktop thumbnail-cache access. It may own capability-local opaque state such as an APNG decoder but no application workflow, navigation, cache policy, source identity, or Qt object.
