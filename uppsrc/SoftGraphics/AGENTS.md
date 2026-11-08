# SoftGraphics AGENTS Guide

## Scope
- Applies to `uppsrc/SoftGraphics` and its entire sub-tree of U++ packages.

## Purpose
SoftGraphics is a virtual graphics device API that uses typed Python as its shader language. It provides:
- Explicit device API (no globals) with OpenGL-like function names
- Typed Python shaders compiled via ByteVM Python to typed PyIR
- Multiple backend targets: CPU validation (PyVM), Vulkan/SPIR-V, Metal/MSL, DX12/DXIL, OpenGL/GLSL, OSL (cinematic)
- CPU backend compiles shaders with PyVM and rasterizes: the vertex stage is
  executed per bound vertex, generic varyings are interpolated, and triangles are
  written to the color attachment (RGBA8) with an optional depth test.

## Architecture

### Core Types
- `SGDevice` — Virtual device abstraction, owns backends
- `SGInstance` — Application-level instance, enumerates devices
- `SGSurface` — Presentation surface (window/swapchain)
- `SGBuffer` — GPU buffer (vertex, index, uniform, storage)
- `SGTexture` — GPU texture (1D, 2D, 3D, cube, array)
- `SGShader` — Compiled shader module (typed Python source → typed PyIR)
- `SGPipeline` — Graphics/compute pipeline state
- `SGFramebuffer` — Render target attachment set
- `SGCommandBuffer` — Recorded command list for submission

### Shader Pipeline
```
Typed Python Source
    ↓ (ByteVM PyCompiler)
Typed PyIR (with type annotations preserved)
    ↓ (ShaderTranspiler)
Backend-specific IR (SPIR-V, MSL, DXIL, GLSL, OSL)
    ↓ (Backend compiler)
GPU executable / CPU interpreter
```

### Type System
Shader language = typed Python subset using `typing` module:
- `int`, `float`, `bool`, `str` — scalar types
- `Vec2`, `Vec3`, `Vec4`, `Mat2`, `Mat3`, `Mat4` — built-in math types (mapped from Geometry)
- `List[T]`, `Dict[K, V]`, `Tuple[...]`, `Optional[T]`, `Union[T1, T2, ...]`
- `TypedDict` — uniform block / push constant layouts
- `Protocol` — shader interface contracts
- `Generic` / `TypeVar` — polymorphic shaders

Type annotations drive:
- Descriptor set / root signature / argument buffer layout
- Vertex attribute / varying interface
- Uniform / push constant / storage buffer layout
- Texture / sampler binding points

## Extension Points
- Add new backend in `ShaderTranspiler` derived class
- Extend `SGDevice` with backend-specific capabilities
- Add shader dialect built-in types/functions in `Shader.cpp` (`RegisterShaderTypes`)
- Implement custom resource types via `SGResource` hierarchy

## Conventions
- Follow `/CODESTYLE.md` for code style
- Use proper U++ naming: PascalCase classes, camelCase methods
- All public APIs documented with Topic++ comments
- Headless-first: every GUI feature has headless twin (see `ai/rules/headless-gui-dual-purpose.md`)
- Use `RecyclerPool` for high-frequency allocations (see `ai/rules/memory-pooling.md`)

## Dependencies
- Core: Basic U++ functionality
- Core/TextParsing: Text parsing utilities
- ByteVM/Python: Typed Python compiler/VM for shaders
- Geometry: Math types (Vec2/3/4, Mat2/3/4)
- Draw/Extensions: Graphics primitives

## Where To Start
- `SoftGraphics.h` — Main header with all public types
- `Device.h/cpp` — Device and instance API
- `Shader.h/cpp` — Shader compilation pipeline
- `ShaderIR.h/cpp` — Typed PyIR representation and transpiler base
- `CpuBackend.h/cpp` — CPU validation backend: compiles/executes typed Python shaders via PyVM; contains the triangle-setup/scanline rasterizer

## CPU backend execution model

- The compiled `PyLambda` carries its own `globals` dict (see `PyVM::Call`), so a
  fresh execution VM's registrations are ignored. `RegisterCpuBackendBuiltins()`
  therefore installs the executable constructors (`Vec2`/`Vec3`/`Vec4`/`Mat*`) and
  GLSL builtin names directly into the shader's own globals after compilation.
- Vector constructors follow GLSL flattening: `Vec4(vec3, float)`, `Vec4(vec2, vec2)`
  and scalar fills all work (`Vec2/Vec3` widen with 0, `Vec4` widens `w` with 1).
- `CpuBackend::Draw` reads positions from the bound vertex buffer (first 3 floats at
  `VertexInput::offset`, stride from the pipeline's `VertexInputDesc`), runs the
  vertex shader, assembles triangles for the pipeline topology, clips to the
  viewport/scissor, fills with perspective-correct barycentric interpolation of
  generic varyings, applies the pipeline depth test, and calls the fragment shader
  per fragment. A fragment shader with no parameters is called with none.
- `DeviceCPU::WaitIdle` replays recorded commands, clears each render-pass color
  attachment (there is no load-op yet) and dispatches draws to `CpuBackend::Draw`.
- Not yet wired: near-plane clipping, points/lines topologies, numeric evaluation
  of the GLSL builtin functions, and the transpiler `Validate()` results.