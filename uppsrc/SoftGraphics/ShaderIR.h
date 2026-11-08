#pragma once

#include "SoftGraphics.h"
#include <ByteVM/Python/Python.h>

namespace SoftGraphics {

using Upp::Array;
using Upp::String;
using Upp::One;
using Upp::PyLambda;

// Shader Intermediate Representation
// Wraps ByteVM typed PyIR and provides transpilation interface

struct TypeAnnotation {
	String name;           // e.g., "int", "float", "Vec3", "List", "Optional"
	Array<TypeAnnotation> typeArgs;  // For generics: List<int> -> name="List", typeArgs=[int]
	bool isOptional = false;  // For Optional[T]
	Array<TypeAnnotation> unionTypes;  // For Union[T1, T2, ...]

	TypeAnnotation() = default;
	TypeAnnotation(const TypeAnnotation&) = delete;
	TypeAnnotation& operator=(const TypeAnnotation&) = delete;
	TypeAnnotation(TypeAnnotation&&) = default;
	TypeAnnotation& operator=(TypeAnnotation&&) = default;
};

struct ShaderParameter {
	String name;
	TypeAnnotation type;
	int binding = -1;       // Descriptor set / binding point
	int set = -1;           // Descriptor set index
	bool isUniform = true;  // Uniform vs storage buffer

	ShaderParameter() = default;
	ShaderParameter(const ShaderParameter&) = delete;
	ShaderParameter& operator=(const ShaderParameter&) = delete;
	ShaderParameter(ShaderParameter&&) = default;
	ShaderParameter& operator=(ShaderParameter&&) = default;
};

struct ShaderInterface {
	Array<ShaderParameter> inputs;      // Vertex attributes, varyings in
	Array<ShaderParameter> outputs;     // Varyings out, fragment outputs
	Array<ShaderParameter> uniforms;    // Uniform buffers, push constants
	Array<ShaderParameter> storageBuffers;  // Storage buffers
	Array<ShaderParameter> textures;    // Sampled textures
	Array<ShaderParameter> samplers;    // Samplers
	Array<ShaderParameter> images;      // Storage images

	ShaderInterface() = default;
	ShaderInterface(const ShaderInterface&) = delete;
	ShaderInterface& operator=(const ShaderInterface&) = delete;
	ShaderInterface(ShaderInterface&&) = default;
	ShaderInterface& operator=(ShaderInterface&&) = default;
};

class ShaderIR {
public:
	ShaderIR() = default;
	virtual ~ShaderIR() = default;

	// Set the compiled ByteVM PyLambda (with type annotations)
	void SetPyLambda(PyLambda* lambda) { pyLambda = lambda; }
	PyLambda* GetPyLambda() const { return pyLambda; }

	// Extract shader interface from type annotations
	virtual ShaderInterface ExtractInterface() = 0;

	// Transpile to backend-specific code
	virtual String TranspileToSPIRV() = 0;
	virtual String TranspileToMSL() = 0;
	virtual String TranspileToDXIL() = 0;
	virtual String TranspileToGLSL() = 0;
	virtual String TranspileToOSL() = 0;

	// CPU backend: execute in PyVM
	virtual void ExecuteVertexShader(const ShaderInterface& interface, void* vertexData, void* outputVaryings) = 0;
	virtual void ExecuteFragmentShader(const ShaderInterface& interface, void* varyingData, void* outputColor) = 0;
	virtual void ExecuteComputeShader(const ShaderInterface& interface, void* workgroupData) = 0;

protected:
	PyLambda* pyLambda = nullptr;
};

// Factory function
One<ShaderIR> CreateShaderIR();

} // namespace SoftGraphics