#pragma once

#include "SoftGraphics.h"
#include "ShaderIR.h"
#include <ByteVM/Python/Python.h>

namespace SoftGraphics {

using Upp::Array;
using Upp::PyVM;
using Upp::PyValue;

// One vertex after the vertex stage: clip-space position plus generic varyings
// (each varying carried as a vec4; a Vec2/Vec3 declared in the shader is widened
// with zero/one fill by the interpreter's vector constructors).
struct VertexOutput : Upp::Moveable<VertexOutput> {
	Upp::vec4 clip = Upp::vec4(0, 0, 0, 1);
	Upp::Vector<Upp::vec4> varyings;
	int inputVertex = -1;   // vertex id in the bound stream, -1 when unbound
};

// A bound vertex stream for one draw call.
struct VertexInput {
	const Upp::byte* data = nullptr;
	size_t size = 0;
	size_t stride = 0;
	size_t offset = 0;
	Upp::uint32 firstVertex = 0;

	bool IsBound() const { return data != nullptr && stride > 0; }
};

// Everything the rasterizer needs for one Draw/DrawIndexed command.
struct DrawParams {
	VertexInput vertexInput;
	Upp::uint32 vertexCount = 0;
	Upp::uint32 instanceCount = 1;
	Upp::uint32 firstVertex = 0;
	PrimitiveTopology topology = PrimitiveTopology::Triangles;
	Viewport viewport;
	Scissor scissor;
	Texture* colorTarget = nullptr;
	Texture* depthTarget = nullptr;
	bool depthTest = true;
	bool depthWrite = true;
	CompareOp depthCompare = CompareOp::Less;

	// Indexed draw (leave indexData null for a non-indexed draw).
	const Upp::byte* indexData = nullptr;
	size_t indexOffset = 0;
	Upp::uint32 indexCount = 0;
};

// CPU validation backend: compiles typed Python shaders with ByteVM's PyVM and
// executes them per vertex/fragment/workgroup, rasterizing into the framebuffer
// color attachment (RGBA8).
class CpuBackend {
public:
	CpuBackend() = default;
	~CpuBackend() = default;

	// Initialize the backend
	bool Initialize();

	// Compile typed Python shader to typed PyIR (via ByteVM)
	ShaderCompileResult CompileShader(const ShaderDesc& desc);

	// Execute the vertex shader for each listed vertex and append one
	// VertexOutput per vertex (clip-space position + generic varyings) to `out`.
	// `vertexIndices` holds the vertex-stream slots to process (sequential for a
	// non-indexed draw, gathered through the index buffer for an indexed draw).
	void ExecuteVertexShader(Shader* shader, Upp::Vector<VertexOutput>& out,
	                         const VertexInput& input, const Upp::Vector<int>& vertexIndices);

	// Execute the fragment shader for a fragment whose interpolated generic
	// varyings are given. Returns false when the shader produced no color.
	bool ExecuteFragmentShader(Shader* shader, const Upp::Vector<Upp::vec4>& varyings,
	                          Upp::vec4& outColor);

	// Execute compute shader
	void ExecuteComputeShader(Shader* shader, const ShaderInterface& interface,
	                          void* workgroupData, int groupCountX, int groupCountY, int groupCountZ);

	// Run a full graphics draw: vertex stage, triangle setup, rasterization,
	// depth test and color write into DrawParams::colorTarget (RGBA8).
	void Draw(Shader* vertexShader, Shader* fragmentShader, const DrawParams& params);

	// Get the PyVM instance for direct access
	PyVM* GetVM() { return &vm; }

private:
	PyVM vm;
	// Keeps compiled shader modules alive (their globals own the PyLambda).
	Upp::Vector<Upp::PyValue> modules;
};

// Install the execution-time constructors (Vec2..Mat4, GLSL builtins) into a
// shader's own globals dict. The compiled PyLambda resolves names against its
// `globals`, so registrations made on a fresh execution VM are not visible.
void RegisterCpuBackendBuiltins(PyValue& globals);

} // namespace SoftGraphics
