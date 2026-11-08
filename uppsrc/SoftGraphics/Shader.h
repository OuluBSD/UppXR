#pragma once

#include "SoftGraphics.h"
#include <ByteVM/Python/Python.h>

namespace SoftGraphics {

class Shader {
public:
	Shader(const ShaderDesc& desc) : desc(desc) {}
	virtual ~Shader() = default;

	const ShaderDesc& GetDesc() const { return desc; }
	virtual PyLambda* GetPyLambda() const = 0;
	virtual ShaderCompileResult GetCompileResult() const = 0;

protected:
	ShaderDesc desc;
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&&) = default;
	Shader& operator=(Shader&&) = default;
};

class ShaderCPU : public Shader {
public:
	ShaderCPU(const ShaderDesc& desc, const ShaderCompileResult& compileResult);
	~ShaderCPU() override = default;

	PyLambda* GetPyLambda() const override { return pyLambda; }
	ShaderCompileResult GetCompileResult() const override { return compileResult; }

private:
	PyLambda* pyLambda = nullptr;
	ShaderCompileResult compileResult;
};

// Compile typed Python shader source into a typed PyLambda.
// The shader dialect registers graphic types (Vec2..Mat4, Sampler*, StorageBuffer,
// PushConstant, ...) as subscriptable typing forms before the module runs, so that
// parameter/return annotations evaluate. Annotations are runtime values, so the
// module is executed once; `owner` retains the entry-point function object and must
// outlive the returned pyLambda (the VM is torn down after compilation).
ShaderCompileResult CompileShaderSource(const ShaderDesc& desc, Upp::PyValue& owner);

} // namespace SoftGraphics