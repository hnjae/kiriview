# C++ And Qt Safety

KiriView uses ISO C++23 and Qt 6 ownership facilities to minimize memory, lifetime, and concurrency hazards in the native application. These practices reduce risk but do not make C++ memory-safe; every API must still make ownership, borrowing, mutation, and thread affinity explicit.

## Language And Toolchain Baseline

- Application-owned C++ sources, C++ tests, and generated CXX bridge translation units compile as ISO C++23 with vendor language extensions disabled. C++17 and C++20 compatibility modes are not supported application targets.
- The C++23 application baseline is independent of Qt's lower language requirement. Application code may use C++23 language and standard-library facilities when they are provided by KiriView's declared minimum compiler and standard library.
- The top-level CMake build owns the minimum compiler, standard-library, Qt 6, and KDE Frameworks 6 dependency baselines. Development, test, lint, packaging, and editor tooling consume those same baselines rather than choosing their own language or dependency compatibility modes.
- Compatibility shims for a lower C++ language mode are not architecture boundaries. Raising a compiler, standard-library, Qt, or KDE Frameworks minimum is an intentional build-boundary change rather than an implicit consequence of an isolated source edit.

## Value-Oriented Policy

- Product policy consumes owned values and produces typed decisions without acquiring QObject identity, signals, or an event-loop dependency solely because it is implemented in C++. The concrete value representation is an implementation choice.
- Qt value types such as `QString`, `QByteArray`, `QUrl`, `QSize`, and `QRect` may be canonical policy values when they match the application domain and retain deterministic value semantics.
- Policy avoids environment-dependent Qt services. Filesystem, clock, locale, plugin, desktop-service, and runtime capability facts enter as explicit values supplied by their owners.
- APIs must preserve type safety and explicit lifetime semantics and use non-deprecated Qt 6 and C++ standard-library facilities. Compatibility wrappers for obsolete Qt or C++ APIs are not architecture boundaries.

## Ownership

- Prefer automatic storage and value members. Non-QObject heap ownership uses an RAII owner whose semantics match the actual lifetime; shared lifetime is used only when multiple owners genuinely extend it, and observers that may outlive an owner can detect expiration. Concrete smart-pointer types are local implementation choices unless they form a declared boundary.
- QObject graphs use one explicit Qt parent owner for heap-allocated children. A QObject must not simultaneously have a parent owner and a competing smart-pointer owner, and stack-allocated QObjects must not be assigned a parent that can delete them.
- Raw pointers and references are non-owning. An owning raw pointer is not an application ownership contract; observations that may survive the current call or queued delivery use a lifetime-aware owner-scoped handle.
- Ordinary ownership must not depend on manually paired raw allocation and release. External C APIs, placement construction, custom deleters, and ownership-transfer APIs are isolated behind narrow RAII adapters that document the required allocator and release operation.
- Representation-changing casts and direct buffer access stay inside codec, rendering, FFI, or external-library adapters and validate size, alignment, format, and lifetime before exposing a typed value.

## Views And Buffers

- `QStringView`, `QByteArrayView`, `std::span`, raw pointer ranges, and similar views never become durable state and never cross an async, queued, cached, or FFI lifetime boundary unless the backing owner is explicitly retained for the complete use.
- Values crossing worker, queued, cache, or public boundaries own their storage. A borrowed input may be used synchronously, but a deferred operation captures an owning value or a declared lifetime handle.
- Indexing, byte counts, strides, dimensions, and integer conversions are validated before access or allocation. Code must not rely on debug-only assertions to establish release-build memory safety.
- A `QImage` or other Qt value created over external storage either owns that storage through a documented cleanup contract or detaches by copying before the external storage can be invalidated.
- Implicit sharing does not make concurrent mutation safe. Cross-thread values are immutable owned values or independently owned copies unless a synchronization owner explicitly governs mutation.

## QObject And Async Lifetimes

- QObject, signal, timer, job, and thread-affinity state belong to runtime, facade, or effect owners rather than plain policy.
- Connections that capture owner state must bind delivery to a lifetime that is invalidated automatically when the owner is destroyed. Other captured objects use value ownership, lifetime-aware observation, or the operation-identity rules from [Async Lifecycle](async-lifecycle.md); durable contextless captures of raw `this` are not allowed.
- A QObject is accessed only from its owning thread unless its API explicitly permits otherwise. Cross-thread work returns owned plain payloads through queued delivery, and QObject destruction that must occur on its affinity thread uses the owning runtime's Qt lifecycle path.
- Cancellation never substitutes for lifetime validation. Queued and worker completions validate both a live owner and the current operation identity before dereferencing owner state or publishing a result.
