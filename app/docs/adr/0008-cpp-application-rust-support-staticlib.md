# C++ Application And Rust Support Static Library

## Context

KiriView is a Qt and KDE application whose QObject lifetimes, QML surface, runtime effects, async ownership, and most production code already live in C++. Making Cargo and CXX-Qt own the application build while broad Qt-independent product policy lives across a Rust/C++ bridge adds generated data models, conversion layers, source inventories, and bidirectional build orchestration without isolating an independent product subsystem.

Four Rust libraries continue to provide capabilities that KiriView requires: `nom-exif` for embedded metadata, `png` for streaming APNG decoding, `resvg` for the supported static SVG subset, and `xdg-thumbnail` for desktop thumbnail-cache interoperability. QtSvg does not implement the SVG behavior required by KiriView, so replacing `resvg` would reduce supported rendering behavior.

## Decision

The KiriView application, product policy, and Qt/KDE runtime are C++. Plain policy remains value-oriented and testable without becoming QObject state solely to share the application language.

CMake owns the application executable, production C++ and QML, generated Qt/KDE artifacts, the repository-internal `ImageViewport` component, and application test targets.

Cargo owns one repository-internal Rust support library built as a `staticlib`. The library provides only the capabilities backed by `nom-exif`, `png`, `resvg`, and `xdg-thumbnail`, plus capability-local adapter logic and opaque state required to expose them safely. It does not own product workflows, navigation, format routing, cache policy, application source identity, Qt objects, or public state.

CXX provides the typed value and opaque-handle bridge where needed. CXX-Qt is not part of the application build or runtime boundary. The Rust library does not call back into C++; C++ invokes support operations with plain values or bytes and retains scheduling, result acceptance, stale-completion rejection, and Qt value construction.

## Consequences

The application has one CMake-owned Qt/KDE build graph while Cargo remains a subordinate producer of the statically linked Rust support artifact and its generated boundary files.

The Rust library has no independent installation, dynamic loading, stable ABI, or release-version contract. KiriView and the support library evolve and link atomically.

Consolidating product policy in C++ does not introduce raw ownership as an application design. The native layer follows the value, RAII, QObject, buffer, callback, and thread-affinity contracts in [C++ And Qt Safety](../architecture/cpp-qt-safety.md).

The `nom-exif`, `png`, `resvg`, and `xdg-thumbnail` dependencies remain part of the application. ADR 0003 remains authoritative for resvg rendering, ADR 0005 remains authoritative for Rust `png` APNG streaming, and ADRs 0006 and 0007 retain their declared consequences for SVG and provider-backed rendering.

ADR 0004 remains authoritative for explicit image-input classification and selected-decoder failure semantics, but this decision supersedes its assignment of classification policy and its tests to Rust. Classification is native C++ product policy.
