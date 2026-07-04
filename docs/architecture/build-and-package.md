# Build And Package Boundary

The supported package target is Linux with a static Qt QML module plugin. The installable package consists of the public C++ header, exported CMake package target, QML module metadata, and static QML plugin archive needed by downstream Linux consumers. Cross-platform plugin filenames, dynamic-plugin layouts, and non-Linux install-consumer portability are outside the target architecture and must not constrain the installed package boundary.

The installed package boundary is narrower than the build tree. Consumers include the public header and link the exported package target; they must not require private headers, source-tree include paths, internal provider transport types, controller internals, render adapter types, scene graph types, native texture handles, private test probes, or build-tree-only QML imports.

Backend dependencies needed by the installed Linux/static package may be expressed as package target link requirements. Those dependencies do not make render backend selection, OpenGL resource ownership, scene graph object lifetime, or native texture injection part of the public API; backend choice remains an internal rendering concern behind the item, public value types, commands, status, and revision tokens.

Install-consumer boundaries may assume the Linux/static plugin shape. Unsupported platform layouts must not introduce compatibility requirements, dependency exposure, or public API commitments for the supported package.
