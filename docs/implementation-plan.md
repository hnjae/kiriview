# Implementation Plan

## Test Cleanup Assumptions

- Backend cancellation and stale-completion tests stay at deterministic injected-worker or token boundaries when production providers do not expose a deterministic post-cancel hook.
- QML `Qt.callLater` negative assertions drain posted events instead of using wall-clock waits.
- Repository artifact and build-wiring policy belongs to `ci:lint:repo-policy`; C++ architecture boundary tests keep durable ownership, API, and source-pattern checks.
- Exhaustive video playback control policy cases stay in Rust; C++ tests cover wrapper dispatch, Rust/C++ conversion, and runtime application of returned plans.
