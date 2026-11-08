#include "Shader.h"
#include "Device.h"
#include <ByteVM/Python/PyCompiler.h>
#include <ByteVM/Python/PyVM.h>
#include <Core/TextParsing/TextParsing.h>

namespace SoftGraphics {

using ::Upp::PyCompiler;
using ::Upp::PyIR;
using ::Upp::PyLambda;
using ::Upp::PyValue;
using ::Upp::PyVM;
using ::Upp::Index;
using ::Upp::String;
using ::Upp::Tokenizer;
using ::Upp::Vector;

ShaderCPU::ShaderCPU(const ShaderDesc& desc, const ShaderCompileResult& compileResult)
	: Shader(desc), compileResult(compileResult) {
	pyLambda = compileResult.pyLambda;
}

Shader* sgCreateShader(Device* device, const ShaderDesc& desc) {
	return device->CreateShader(desc);
}

ShaderCompileResult sgCompileShader(Device* device, const ShaderDesc& desc) {
	return device->CompileShader(desc);
}

void sgDestroyShader(Shader* shader) {
	delete shader;
}

// Register the shader dialect's built-in type names. Each becomes a subscriptable
// typing form (Sampler2D, StorageBuffer[float] -> StorageBuffer, ...) so that the
// type annotations in a shader definition evaluate without further imports.
static void RegisterShaderTypes(PyVM& vm) {
	static const char* const names[] = {
		"Vec2", "Vec3", "Vec4",
		"Mat2", "Mat3", "Mat4",
		"IVec2", "IVec3", "IVec4",
		"Sampler1D", "Sampler2D", "Sampler3D", "SamplerCube", "Sampler2DArray",
		"StorageBuffer", "UniformBuffer", "PushConstant", "StorageImage",
		"void",
	};
	PyValue& globals = vm.GetGlobalsRW();
	for(const char* name : names)
		globals.GetDictRW().GetAdd(PyValue(name)) = PyValue::TypingForm(name);

	// Built-in shader functions (GLSL/HLSL-style). They are registered so that a
	// shader body referencing them type-checks; actual evaluation is the job of the
	// backend that interprets the PyIR.
	static const char* const builtins[] = {
		"normalize", "length", "distance", "dot", "cross", "reflect", "refract",
		"faceforward", "mix", "clamp", "smoothstep", "step", "min", "max",
		"abs", "sign", "floor", "ceil", "fract", "round", "trunc", "mod",
		"pow", "sqrt", "inversesqrt", "exp", "exp2", "log", "log2",
		"sin", "cos", "tan", "asin", "acos", "atan", "atan2",
		"transpose", "inverse", "determinant",
	};
	for(const char* name : builtins)
		if(globals.GetDictRW().Find(PyValue(name)) < 0)
			globals.GetDictRW().GetAdd(PyValue(name)) = PyValue::Function(name);
}

// Reject shaders that reference names which are neither builtins, freshly defined
// module globals, function parameters, nor names bound inside the function body.
// This catches typos like `invalid_function()` before transpilation.
static bool ValidateLambda(PyLambda* lambda, const PyValue& globals, const String& entry, String& error) {
	if(!lambda) return true;

	Index<String> known;
	if(globals.GetType() == ::Upp::PY_DICT) {
		const auto& d = globals.GetDict();
		for(int i = 0; i < d.GetCount(); i++)
			known.Add(d.GetKey(i).ToString());
	}
	for(const String& arg : lambda->arg)
		known.Add(arg);
	for(const PyIR& ins : lambda->ir)
		if(ins.code == ::Upp::PY_STORE_FAST || ins.code == ::Upp::PY_STORE_NAME || ins.code == ::Upp::PY_STORE_GLOBAL)
			known.Add(ins.arg.GetStr().ToString());

	for(const PyIR& ins : lambda->ir) {
		if(ins.code != ::Upp::PY_LOAD_NAME && ins.code != ::Upp::PY_LOAD_GLOBAL)
			continue;
		String name = ins.arg.GetStr().ToString();
		if(known.Find(name) < 0) {
			error = "Undefined name '" + name + "' in shader entry point '" + entry + "'";
			return false;
		}
	}
	return true;
}

ShaderCompileResult CompileShaderSource(const ShaderDesc& desc, PyValue& owner) {
	ShaderCompileResult result;

	Tokenizer tk;
	tk.SkipPythonComments();
	if(!tk.Process(desc.source, "shader.py")) {
		result.error = "Tokenization failed";
		return result;
	}
	tk.CombineTokens();
	tk.NewlineToEndStatement();

	PyCompiler compiler(tk.GetTokens(), "shader.py");
	Vector<PyIR> ir;
	try {
		compiler.Compile(ir);
	}
	catch(::Upp::Exc& e) {
		result.error = "Compile error: " + e.ToString();
		return result;
	}

	// Annotations are runtime values attached by PY_ANNOTATE_FUNC, so the module
	// has to execute once to materialize them on the function objects.
	PyVM vm;
	RegisterShaderTypes(vm);
	vm.SetIR(ir);
	try {
		vm.Run();
	}
	catch(::Upp::Exc& e) {
		result.error = "Runtime error: " + e.ToString();
		return result;
	}

	PyValue globals = vm.GetGlobals();
	PyValue entry = globals.GetItem(PyValue(desc.entryPoint));
	if(!entry.IsFunction()) {
		result.error = "Entry point '" + desc.entryPoint + "' not found";
		return result;
	}

	PyLambda* lambda = &entry.GetLambdaRW();
	if(!ValidateLambda(lambda, globals, desc.entryPoint, result.error))
		return result;

	// Retain the function object itself: ~PyVM tears down its globals graph, which
	// would otherwise release the lambda we hand out. `owner` must outlive pyLambda.
	owner = entry;

	result.success = true;
	result.pyLambda = lambda;
	return result;
}

} // namespace SoftGraphics
