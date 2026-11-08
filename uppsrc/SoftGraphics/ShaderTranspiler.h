#pragma once

#include "SoftGraphics.h"
#include "ShaderIR.h"

namespace SoftGraphics {

using Upp::String;
using Upp::PyLambda;
using Upp::PyValue;

// Transpiler base class - converts typed PyIR to backend-specific code
class ShaderTranspiler {
public:
	ShaderTranspiler() = default;
	virtual ~ShaderTranspiler() = default;

	// Transpile typed PyLambda to target shader language
	virtual String Transpile(PyLambda* pyLambda, const ShaderInterface& interface) = 0;

	// Validate the transpiled code
	virtual bool Validate(const String& code) = 0;

public:
	// Helper: extract type annotations from PyLambda
	ShaderInterface ExtractInterface(PyLambda* pyLambda);

	// Helper: parse type annotation from PyValue
	TypeAnnotation ParseTypeAnnotation(const PyValue& value);

	// Helper: map TypeAnnotation to target type string
	virtual String MapType(const TypeAnnotation& type) = 0;

	// Helper: generate uniform/descriptor layout
	virtual String GenerateLayout(const ShaderInterface& interface) = 0;

protected:
	// Walk PyIR and emit target code (overridden by each backend)
	String EmitTemp(int idx) { return "t" + Upp::AsString(idx); }
};

// Temporary variable for SSA-style code generation
struct TempVar : Upp::Moveable<TempVar> {
	int nameId = 0;
	int typeId = 0;
	bool isConst = false;
	
	TempVar() = default;
	TempVar(int n, int t, bool c) : nameId(n), typeId(t), isConst(c) {}
};

// Concrete transpilers
class SPIRVTranspiler : public ShaderTranspiler {
public:
    String Transpile(PyLambda* pyLambda, const ShaderInterface& interface) override;
    bool Validate(const String& code) override;
    String MapType(const TypeAnnotation& type) override;
    String GenerateLayout(const ShaderInterface& interface) override;

private:
    String EmitSPIRV(PyLambda* pyLambda, const ShaderInterface& interface);
};

class MSLTranspiler : public ShaderTranspiler {
public:
    String Transpile(PyLambda* pyLambda, const ShaderInterface& interface) override;
    bool Validate(const String& code) override;
    String MapType(const TypeAnnotation& type) override;
    String GenerateLayout(const ShaderInterface& interface) override;

private:
    String EmitMSL(PyLambda* pyLambda, const ShaderInterface& interface);
    String MapOpToMSL(int code, const String& a, const String& b);
};

class DXILTranspiler : public ShaderTranspiler {
public:
    String Transpile(PyLambda* pyLambda, const ShaderInterface& interface) override;
    bool Validate(const String& code) override;
    String MapType(const TypeAnnotation& type) override;
    String GenerateLayout(const ShaderInterface& interface) override;

private:
    String EmitHLSL(PyLambda* pyLambda, const ShaderInterface& interface);
    String MapOpToHLSL(int code, const String& a, const String& b);
};

class GLSLTranspiler : public ShaderTranspiler {
public:
	String Transpile(PyLambda* pyLambda, const ShaderInterface& interface) override;
	bool Validate(const String& code) override;
	String MapType(const TypeAnnotation& type) override;
	String GenerateLayout(const ShaderInterface& interface) override;
	
private:
	String EmitGLSL(PyLambda* pyLambda, const ShaderInterface& interface);
	String MapOpToGLSL(int code, const String& a, const String& b);
};

class OSLTranspiler : public ShaderTranspiler {
public:
    String Transpile(PyLambda* pyLambda, const ShaderInterface& interface) override;
    bool Validate(const String& code) override;
    String MapType(const TypeAnnotation& type) override;
    String GenerateLayout(const ShaderInterface& interface) override;

private:
    String EmitOSL(PyLambda* pyLambda, const ShaderInterface& interface);
    String MapOpToOSL(int code, const String& a, const String& b);
};

} // namespace SoftGraphics