#include "ShaderIR.h"
#include <ByteVM/Python/PyValue.h>
#include <ByteVM/Python/PyVM.h>

namespace SoftGraphics {

using Upp::PyValue;

class ShaderIRImpl : public ShaderIR {
public:
	ShaderIRImpl() = default;
	~ShaderIRImpl() override = default;

	ShaderInterface ExtractInterface() override {
		ShaderInterface interface;
		if (!pyLambda) return interface;

		// Extract type annotations from PyLambda.annotations. The map stores each
		// parameter's declared type (and the key "return" for the return type).
		for (int i = 0; i < pyLambda->annotations.GetCount(); ++i) {
			const String& name = pyLambda->annotations.GetKey(i);
			const PyValue& annValue = pyLambda->annotations[i];

			TypeAnnotation typeAnn = ParseTypeAnnotation(annValue);

			ShaderParameter param;
			param.name = name;
			param.type = std::move(typeAnn);

			const String& tn = param.type.name;
			if (name == "return")
				interface.outputs.Add(std::move(param));
			else if (tn.StartsWith("Sampler"))
				interface.textures.Add(std::move(param));
			else if (tn == "StorageBuffer" || tn == "StorageImage")
				interface.storageBuffers.Add(std::move(param));
			else if (IsShaderPrimitive(tn))
				interface.inputs.Add(std::move(param));
			else
				// TypedDict/struct parameters and anything else are treated as a
				// uniform block.
				interface.uniforms.Add(std::move(param));
		}

		return interface;
	}

	String TranspileToSPIRV() override { return "// SPIR-V transpilation not implemented"; }
	String TranspileToMSL() override { return "// MSL transpilation not implemented"; }
	String TranspileToDXIL() override { return "// DXIL transpilation not implemented"; }
	String TranspileToGLSL() override { return "// GLSL transpilation not implemented"; }
	String TranspileToOSL() override { return "// OSL transpilation not implemented"; }

	void ExecuteVertexShader(const ShaderInterface& interface, void* vertexData, void* outputVaryings) override {
		if (!pyLambda) return;
		// Execute vertex shader entry point in PyVM
		// This would call the compiled PyLambda with vertex data
	}

	void ExecuteFragmentShader(const ShaderInterface& interface, void* varyingData, void* outputColor) override {
		if (!pyLambda) return;
		// Execute fragment shader entry point in PyVM
	}

	void ExecuteComputeShader(const ShaderInterface& interface, void* workgroupData) override {
		if (!pyLambda) return;
		// Execute compute shader entry point in PyVM
	}

private:
	static bool IsShaderPrimitive(const String& n) {
		static const char* const prims[] = {
			"Vec2", "Vec3", "Vec4", "IVec2", "IVec3", "IVec4",
			"Mat2", "Mat3", "Mat4", "int", "uint", "float", "bool", "void",
		};
		for(const char* p : prims)
			if(n == p)
				return true;
		return false;
	}

	TypeAnnotation ParseTypeAnnotation(const PyValue& value) {
		TypeAnnotation result;
		if (value.GetType() == Upp::PY_STR) {
			result.name = value.GetStr().ToString();
		}
		else if (value.GetType() == Upp::PY_USERDATA && value.IsUserDataValid()) {
			// Typing forms report "typing.<Name>"; strip the module qualifier so
			// Vec3, StorageBuffer, Sampler2D, ... come back as bare names.
			String tn = value.GetUserData().GetTypeName();
			if (tn.StartsWith("typing."))
				tn = tn.Mid(7);
			result.name = tn;
		}
		else if (value.GetType() == Upp::PY_TUPLE) {
			// A tuple return/annotation, e.g. -> (Vec4, Vec2): record the first
			// element's name; structured returns are expanded by the caller.
			int n = value.GetCount();
			if (n > 0) {
				TypeAnnotation first = ParseTypeAnnotation(value.GetItem(0));
				result.name = first.name;
			}
		}
		return result;
	}
};

One<ShaderIR> CreateShaderIR() {
	return new ShaderIRImpl();
}

} // namespace SoftGraphics