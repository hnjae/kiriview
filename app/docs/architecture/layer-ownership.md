# Layer Ownership

QML and Kirigami own declarative UI composition:

- Page structure, menus, toolbars, overlays, and UI-local shortcut attachment points.
- Visual placement of action objects supplied by the application facade.
- Bindings to documented QObject properties and invokable methods.
- UI-local state expressly assigned to QML by an owner contract. When that state affects shared behavior, QML reports its current facts through the named owner input instead of applying the behavior itself.

QObject and QQuickItem facade classes own the public QML surface:

- `Q_OBJECT`, `Q_PROPERTY`, `Q_INVOKABLE`, signals, and QML registration.
- Conversion between QML-friendly types and owner-facing application APIs.
- Thin forwarding to the responsible owners and integration surfaces.

Application facade classes, including the KiriView image-viewport facade, are the only QML-facing application API boundaries. API visibility does not transfer domain ownership: KiriView QML forwards raw image interaction facts through the ImageViewport integration owner and does not invoke the dependency directly. Domain behavior must live in the appropriate application runtime or policy owner instead of growing facade-owned workflow state.

One production runtime-composition authority completes the application facade and domain-owner graph before QML can dispatch commands through it, regardless of where a facade object is physically instantiated. QML may bind composed collaborators into toolkit-required visual properties and report UI-local facts, but component-completion callbacks or other imperative UI choreography must not own the runtime attachment sequence.

Application runtime and policy responsibilities belong to named owners:

- `QObject` lifetime, signal delivery, thread affinity, and cancellation.
- `QUrl`, `QImage`, `QAction`, application QML facades, image-provider entries, and other application-owned Qt objects.
- OS and desktop-environment observers such as system-memory discovery and power-saver portal state.
- `QAction` identity, KDE action collections, configured shortcut state, and shortcut persistence.
- KIO jobs, KDE settings, dialogs, notifications, file operations, and runtime integration.
- Localized user-facing strings for dialogs, menus, actions, notifications, desktop metadata, and other UI surfaces. Localization integration consumes installed translation catalogs and KDE/Qt language settings while preserving the English fallback contract.
- Supported-media capability modules expose canonical extension, MIME-type, and collection-kind facts consumed by routing, open-dialog filters, desktop-file advertising, adjacent navigation eligibility, thumbnail eligibility, and decoder-route selection. Image-format policy owns advertised image extension and MIME-type metadata plus decoder-family selection as defined by [Extension Contracts](extension-contracts.md#decoder-contracts). The supported-media capability boundary publishes canonical direct-video extension and MIME-type facts, and collection-access owners publish collection-kind and entry-playability capabilities. Qt/KDE adapters consume those facts and own localized file-dialog labels; no consumer may maintain a divergent supported-media list or become a second format-policy owner.
- The ImageViewport integration owner as the image-document runtime's application adapter for selected source identity, page pairing, reading and scan policy, supported target and command submission, opaque correlation, failure-reference resolution, and KiriView-facing projection.
- Image provider-resource owners as owners of source access, decoded payloads, application cache and display-store entries, predecode and refinement work, typed failure records, provider sessions, payload leases, and supported ownership callbacks only.
- Image presentation through the supported `ImageViewport` boundary. KiriView enforces its own cache, display-store, and source-work budgets behind the provider boundary but must not bypass the dependency with an application-owned presentation or rendering path.

Policy owners hold application decisions that can be computed from values without runtime side effects:

- State transitions and workflow decisions.
- Navigation, page pairing, scan and nearest-point planning, deletion-target, follow-up, cache decisions, format inspection, and decoder routing when they can be computed from coherent owned values.
- Pure calculations where the same input produces the same output.

The Rust support static library owns only the implementations behind the canonical [Rust Support Boundary](rust-support-boundary.md) capability list. It may own capability-local opaque state such as an APNG decoder but no application workflow, navigation, cache policy, source identity, or Qt object.
