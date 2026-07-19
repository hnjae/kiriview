# Layer Ownership

QML and Kirigami own declarative UI composition:

- Page structure, menus, toolbars, overlays, and UI-local shortcut attachment points.
- Visual placement of action objects supplied by the C++ facade.
- Bindings to documented QObject properties and invokable methods.
- UI-only state that does not affect application behavior.

C++ QObject and QQuickItem facade classes own the public QML surface:

- `Q_OBJECT`, `Q_PROPERTY`, `Q_INVOKABLE`, signals, and QML registration.
- Conversion between QML-friendly types and internal controller APIs.
- Thin forwarding to controllers and render items.

Facade classes, including the declared repository-internal `ImageViewport` item surface, are the only QML-facing API boundaries. API visibility does not transfer domain ownership: KiriView QML forwards raw image interaction facts through the image-viewport integration owner and does not invoke component mutations directly. Domain behavior must live in the appropriate runtime, presentation, rendering, or policy owner instead of growing facade-owned workflow state.

C++ Qt/KDE runtime code owns platform integration and side effects and consumes policy from the named owners:

- `QObject` lifetime, signal delivery, thread affinity, and cancellation.
- `QUrl`, `QImage`, `QAction`, `QQuickItem`, image-provider entries, and other Qt objects.
- OS and desktop-environment observers such as system-memory discovery and power-saver portal state.
- `QAction` identity, KDE action collections, configured shortcut state, and shortcut persistence.
- KIO jobs, KDE settings, dialogs, notifications, file operations, and runtime integration.
- Localized user-facing strings for dialogs, menus, actions, notifications, desktop metadata, and other UI surfaces. Localization integration consumes installed translation catalogs and KDE/Qt language settings while preserving the English fallback contract.
- Supported-media capability modules expose canonical extension, MIME-type, and collection-kind facts consumed by routing, open-dialog filters, desktop-file advertising, adjacent navigation eligibility, thumbnail eligibility, and decoder-route selection. Rust image-format policy owns advertised image extension and MIME-type metadata plus decoder-family selection as defined by [Extension Contracts](extension-contracts.md#decoder-contracts). The supported-media capability boundary publishes canonical direct-video extension and MIME-type facts, and collection-access owners publish collection-kind and entry-playability capabilities. C++ adapters consume those facts, adapt them to Qt/KDE surfaces, and own localized file-dialog labels; no consumer may maintain a divergent supported-media list or become a second format-policy owner.
- The repository-internal `ImageViewport` component as the single owner of accepted and retained image presentation, fit and zoom, pan, rotation and mirroring, spread geometry, visible source geometry, per-role animation playback, render admission, and image scene graph resources.
- The image-document integration owner as the application adapter for selected source identity, page pairing, reading and scan policy, component target and command submission, generation correlation, failure-reference resolution, and KiriView-facing projection.
- Image provider-resource owners as owners of source access, decoded payloads, application cache and display-store entries, predecode and refinement work, typed failure records, provider sessions, frame-handle leases, and failure-handle release callbacks only.
- Image render-context adaptation through the component item and its private rendering implementation. The component owns viewport payload admission, backend caps, allocation state, and per-role display budgets and publishes the applicable facts through provider demand. KiriView enforces application cache, display-store, and source-work budgets behind the provider boundary but must not own image textures, scene graph nodes, render commits, or image-load acknowledgements.

Rust owns Qt-independent policy and algorithms:

- State transitions, workflow plans, and reducer-like decisions.
- Navigation, page pairing, scan and nearest-point planning, deletion-target, follow-up, and cache decisions when they are computed from plain value snapshots. Component-owned zoom, viewport geometry, and payload-demand projection do not move into application Rust policy.
- Parsing and byte-level format inspection that is independent of Qt objects.
- Pure calculations where the same input produces the same output.
