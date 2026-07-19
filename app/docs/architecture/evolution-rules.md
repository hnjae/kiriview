# Evolution Rules

The architecture must preserve these invariants as its internal structure evolves:

1. Every workflow value has one canonical owner; derived projections and caches must not become alternate mutable authorities.
1. Qt-independent policy crosses the Rust/C++ boundary only as coherent value-based decisions, while Qt/KDE effects and object lifetimes remain in C++.
1. The FFI boundary exposes explicit typed snapshots, events, plans, and results rather than property-shaped calls or isolated boolean branches.
1. QObject and QML API exposure remains behind C++ facade boundaries and does not transfer domain ownership to the facade or UI.
1. Internal formats have one authoritative schema unless an explicit product or interoperability contract requires compatibility behavior.
