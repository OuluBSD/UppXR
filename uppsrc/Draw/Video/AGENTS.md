AGENTS

Scope
- Applies to `uppsrc/Draw/Video`.

Purpose
- Shared headless video/camera interfaces and types.
- Base package for Draw/Camera and ComputerVision to avoid duplication.

Guidelines
- No CtrlLib or UI code.
- Keep interfaces explicit, verbose, and debuggable.
- Prefer clear data ownership for high-performance pipelines.

Data Ownership Rules
- `VideoFrame::img` is owned by the frame and is the preferred safe path.
- `VideoFrame::planes` is a non-owning view; callers must keep buffers alive until processed.
- `planes` and `img` may both be set when `img` is a decoded copy; `format` must match `img` if used.
- `size` must be set for plane-based frames even if `img` is empty.
- `timestamp_us` is monotonic when possible; set `last_timestamp_us` in stats.
