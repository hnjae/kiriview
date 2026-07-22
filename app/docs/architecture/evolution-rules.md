# Evolution Rules

The architecture must preserve these invariants as its internal structure evolves:

1. Every workflow value has one canonical owner; derived projections and caches must not become alternate mutable authorities.
1. Product policy and authoritative application state remain in C++, including policy that is independent of Qt runtime objects.
1. The Rust/C++ boundary exposes only the support capabilities named by [Language Boundary](language-boundary.md) through explicit typed values, results, and opaque capability-local handles.
1. Rust support code does not call back into C++, own application lifecycle or source identity, or expose property-shaped product decisions.
1. Native code uses one explicit owner per allocation, non-owning raw pointers only within a proven lifetime, context-bound Qt callbacks, owned cross-boundary payloads, and value-oriented policy as defined by [C++ And Qt Safety](cpp-qt-safety.md).
1. QObject and QML API exposure remains behind C++ facade boundaries and does not transfer domain ownership to the facade or UI.
1. Internal formats have one authoritative schema unless an explicit product or interoperability contract requires compatibility behavior.
