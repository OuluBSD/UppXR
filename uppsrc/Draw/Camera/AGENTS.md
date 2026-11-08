AGENTS

Scope
- Applies to `uppsrc/Draw/Camera`.

Purpose
- Headless camera capture + processing core (no UI).
- Hosts camera backends (SoftHMD, V4L2, future Windows/Android).

Guidelines
- Keep UI types out of Draw/Camera (no CtrlLib).
- Place backend implementations here; expose a minimal API for frames, stats, and effects.
- Keep effects as adapters; ComputerVision algorithms remain in `uppsrc/ComputerVision`.
