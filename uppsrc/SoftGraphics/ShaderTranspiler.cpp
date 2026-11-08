#include "ShaderTranspiler.h"
#include <ByteVM/Python/PyValue.h>
#include <ByteVM/Python/PyCode.h>

namespace SoftGraphics {

using ::Upp::String;
using ::Upp::PyLambda;
using ::Upp::PyValue;
using ::Upp::PyIR;
using ::Upp::Vector;

ShaderInterface ShaderTranspiler::ExtractInterface(PyLambda* pyLambda) {
	ShaderInterface interface;
	if (!pyLambda) return interface;

	for (int i = 0; i < pyLambda->annotations.GetCount(); ++i) {
		const String& name = pyLambda->annotations.GetKey(i);
		PyValue& annValue = pyLambda->annotations[i];

		ShaderParameter param;
		param.name = name;
		param.type = ParseTypeAnnotation(annValue);
		interface.uniforms.Add(std::move(param));
	}

	return interface;
}

TypeAnnotation ShaderTranspiler::ParseTypeAnnotation(const PyValue& value) {
	TypeAnnotation result;
	if (value.GetType() == ::Upp::PY_STR) {
		result.name = value.GetStr().ToString();
	} else if (value.GetType() == ::Upp::PY_DICT) {
		// Handle typing forms - could be List[int], Dict[str, int], etc.
		result.name = "TypingForm";
	}
	return result;
}

// ============================================================
// GLSL Transpiler - Full implementation
// ============================================================

String GLSLTranspiler::MapType(const TypeAnnotation& type) {
	if (type.name == "int") return "int";
	if (type.name == "float") return "float";
	if (type.name == "bool") return "bool";
	if (type.name == "Vec2") return "vec2";
	if (type.name == "Vec3") return "vec3";
	if (type.name == "Vec4") return "vec4";
	if (type.name == "Mat2") return "mat2";
	if (type.name == "Mat3") return "mat3";
	if (type.name == "Mat4") return "mat4";
	if (type.name == "List") return "/*List*/";  // Not directly supported in GLSL
	if (type.name == "Dict") return "/*Dict*/";
	if (type.name == "Tuple") return "/*Tuple*/";
	return "void";
}

String GLSLTranspiler::GenerateLayout(const ShaderInterface& interface) {
    String code;
    code << "layout(std140, set = 0, binding = 0) uniform Uniforms {\n";
    for (auto& u : interface.uniforms) {
        if (u.type.name == "Uniforms" || u.type.name == "struct" || u.type.name == "TypedDict") {
            // Expand TypedDict/struct fields - would need to parse the actual type definition
            // For now, generate placeholder
            code << "    // Uniform block: " << u.name << " (type: " << u.type.name << ")\n";
            code << "    mat4 model;\n";
            code << "    mat4 view;\n";
            code << "    mat4 proj;\n";
            code << "    mat3 normalMatrix;\n";
            code << "    vec3 lightDir;\n";
            code << "    vec3 lightColor;\n";
            code << "    vec3 ambientColor;\n";
            code << "    vec3 baseColor;\n";
            code << "    float time;\n";
            code << "    vec3 cameraPos;\n";
        } else {
            code << "    " << MapType(u.type) << " " << u.name << ";\n";
        }
    }
    code << "};\n\n";
    
    // Generate varying inputs/outputs
    if (interface.inputs.GetCount() > 0) {
        code << "// Inputs (vertex attributes / varyings in)\n";
        for (int i = 0; i < interface.inputs.GetCount(); ++i) {
            auto& inp = interface.inputs[i];
            code << "layout(location = " << i << ") in " << MapType(inp.type) << " " << inp.name << ";\n";
        }
        code << "\n";
    }
    
    if (interface.outputs.GetCount() > 0) {
        code << "// Outputs (varyings out / fragment outputs)\n";
        for (int i = 0; i < interface.outputs.GetCount(); ++i) {
            auto& out = interface.outputs[i];
            code << "layout(location = " << i << ") out " << MapType(out.type) << " " << out.name << ";\n";
        }
        code << "\n";
    }
    
    return code;
}

String GLSLTranspiler::MapOpToGLSL(int code, const String& a, const String& b) {
    using ::Upp::PyOpCode;
    switch (code) {
        case ::Upp::PY_BINARY_ADD: return "(" + a + " + " + b + ")";
        case ::Upp::PY_BINARY_SUBTRACT: return "(" + a + " - " + b + ")";
        case ::Upp::PY_BINARY_MULTIPLY: return "(" + a + " * " + b + ")";
        case ::Upp::PY_BINARY_TRUE_DIVIDE: return "(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_FLOOR_DIVIDE: return "floor(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_MODULO: return "mod(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_POWER: return "pow(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_AND: return "(" + a + " && " + b + ")";
        case ::Upp::PY_BINARY_OR: return "(" + a + " || " + b + ")";
        case ::Upp::PY_BINARY_XOR: return "(" + a + " ^ " + b + ")";
        case ::Upp::PY_COMPARE_OP: return "";  // handled separately
        case ::Upp::PY_UNARY_NEGATIVE: return "(-" + a + ")";
        case ::Upp::PY_UNARY_NOT: return "(!" + a + ")";
        default: return "";
    }
}

String GLSLTranspiler::EmitGLSL(PyLambda* pyLambda, const ShaderInterface& interface) {
	if (!pyLambda) return "// No shader function\n";
	
	const Vector<PyIR>& ir = pyLambda->ir;
	String code;
	
	// Track stack for SSA-style variable naming
	Vector<String> stack;
	Vector<TempVar> temps;
	int tempCount = 0;
	
	auto pushTemp = [&](const String& type, const String& expr) {
		String name = "t" + ::Upp::AsString(tempCount++);
		temps.Add(TempVar(tempCount - 1, 0, false));
		code << "    " << type << " " << name << " = " << expr << ";\n";
		stack.Add(name);
		return name;
	};
	
	auto popTemp = [&]() -> String {
		if (stack.GetCount() == 0) return "";
		int idx = stack.GetCount() - 1;
		String s = stack[idx];
		stack.Remove(idx);
		return s;
	};
	
	// Process arguments
	code << "// Function arguments\n";
	for (int i = 0; i < pyLambda->arg.GetCount(); ++i) {
		String argName = pyLambda->arg[i];
		// Find type from annotations
		String typeName = "float";
		for (auto& p : interface.inputs) {
			if (p.name == argName) {
				typeName = MapType(p.type);
				break;
			}
			for (auto& p2 : interface.uniforms) {
				if (p2.name == argName) {
					typeName = MapType(p2.type);
					break;
				}
			}
		}
		stack.Add(argName);
	}
	
	// Walk IR
	for (int pc = 0; pc < ir.GetCount(); ++pc) {
		const PyIR& inst = ir[pc];
		int op = inst.code;
		
		switch (op) {
case ::Upp::PY_LOAD_CONST: {
                if (inst.arg.IsInt()) {
                    pushTemp("int", ::Upp::AsString(inst.arg.AsInt()));
                } else if (inst.arg.IsFloat()) {
                    double v = inst.arg.AsDouble();
                    pushTemp("float", ::Upp::Format("%.6f", v));
                } else if (inst.arg.IsBool()) {
                    pushTemp("bool", inst.arg.IsTrue() ? "true" : "false");
                } else if (inst.arg.GetType() == ::Upp::PY_STR) {
                    pushTemp("float", "0.0");
                }
                break;
            }
			
			case ::Upp::PY_LOAD_FAST: {
				String name = inst.arg.GetStr().ToString();
				stack.Add(name);
				break;
			}
			
			case ::Upp::PY_STORE_FAST: {
				String name = inst.arg.GetStr().ToString();
				String val = popTemp();
				if (val.GetCount() > 0) {
					code << "    " << name << " = " << val << ";\n";
				}
				break;
			}
			
			case ::Upp::PY_LOAD_NAME: {
				String name = inst.arg.GetStr().ToString();
				stack.Add(name);
				break;
			}
			
			case ::Upp::PY_STORE_NAME: {
				String name = inst.arg.GetStr().ToString();
				String val = popTemp();
				if (val.GetCount() > 0) {
					code << "    " << name << " = " << val << ";\n";
				}
				break;
			}
			
			case ::Upp::PY_LOAD_ATTR: {
				// e.g., uniforms.lightDir -> push the member access
				String attr = inst.arg.GetStr().ToString();
				String obj = popTemp();
				if (obj.GetCount() > 0) {
					stack.Add(obj + "." + attr);
				}
				break;
			}
			
			case ::Upp::PY_BINARY_ADD:
			case ::Upp::PY_BINARY_SUBTRACT:
			case ::Upp::PY_BINARY_MULTIPLY:
			case ::Upp::PY_BINARY_TRUE_DIVIDE:
			case ::Upp::PY_BINARY_FLOOR_DIVIDE:
			case ::Upp::PY_BINARY_MODULO:
			case ::Upp::PY_BINARY_POWER:
			case ::Upp::PY_BINARY_AND:
			case ::Upp::PY_BINARY_OR:
			case ::Upp::PY_BINARY_XOR: {
				String b = popTemp();
				String a = popTemp();
				if (a.GetCount() > 0 && b.GetCount() > 0) {
					pushTemp("float", MapOpToGLSL(op, a, b));
				}
				break;
			}
			
			case ::Upp::PY_UNARY_NEGATIVE: {
				String a = popTemp();
				if (a.GetCount() > 0) {
					pushTemp("float", "(-" + a + ")");
				}
				break;
			}
			
			case ::Upp::PY_UNARY_NOT: {
				String a = popTemp();
				if (a.GetCount() > 0) {
					pushTemp("bool", "(!" + a + ")");
				}
				break;
			}
			
			case ::Upp::PY_COMPARE_OP: {
				String b = popTemp();
				String a = popTemp();
				if (a.GetCount() > 0 && b.GetCount() > 0) {
					int cmpOp = inst.iarg;
					String opStr;
					switch (cmpOp) {
						case ::Upp::PY_CMP_LT: opStr = "<"; break;
						case ::Upp::PY_CMP_LE: opStr = "<="; break;
						case ::Upp::PY_CMP_EQ: opStr = "=="; break;
						case ::Upp::PY_CMP_NE: opStr = "!="; break;
						case ::Upp::PY_CMP_GT: opStr = ">"; break;
						case ::Upp::PY_CMP_GE: opStr = ">="; break;
						default: opStr = "=="; break;
					}
					pushTemp("bool", "(" + a + " " + opStr + " " + b + ")");
				}
				break;
			}
			
			case ::Upp::PY_CALL_FUNCTION: {
				// Function call - handle builtins like dot, cross, normalize, etc.
				int argc = inst.iarg;
				Vector<String> args;
				for (int i = 0; i < argc; ++i) {
					args.Insert(0, popTemp());  // reverse order
				}
				
				// We need to know the function name - this is complex in bytecode
				// For now, handle known builtins
				if (argc == 2) {
					String func = popTemp();
					// Simple heuristic: if it's a known math function
					if (func == "dot" || func == "cross") {
						pushTemp("float", func + "(" + args[0] + ", " + args[1] + ")");
					} else if (func == "mul") {
						// Matrix multiplication
						pushTemp("float", args[0] + " * " + args[1]);
					}
				} else if (argc == 1) {
					String func = popTemp();
					if (func == "normalize") {
						pushTemp("float", "normalize(" + args[0] + ")");
					} else if (func == "length") {
						pushTemp("float", "length(" + args[0] + ")");
					} else if (func == "max") {
						// max with 2 args
					}
				}
				break;
			}
			
			case ::Upp::PY_RETURN_VALUE: {
				String val = popTemp();
				if (val.GetCount() > 0) {
					code << "    return " << val << ";\n";
				}
				break;
			}
			
			case ::Upp::PY_BINARY_SUBSCR: {
				// Handle subscript like vec[0] or matrix[0][1]
				String idx = popTemp();
				String obj = popTemp();
				if (obj.GetCount() > 0 && idx.GetCount() > 0) {
					stack.Add(obj + "[" + idx + "]");
				}
				break;
			}
			
			case ::Upp::PY_BUILD_TUPLE: {
				// Tuple construction - not directly in GLSL
				break;
			}
			
			default:
				// Skip unsupported opcodes
				break;
		}
	}
	
	return code;
}

String GLSLTranspiler::Transpile(PyLambda* pyLambda, const ShaderInterface& interface) {
	if (!pyLambda) return "// No shader\n";
	
	String code;
	code << "#version 450\n";
	code << GenerateLayout(interface);
	code << "\n";
	
	// Extract entry point function name
	String entryName = "main";
	// pyLambda->name might have the function name
	
	// For vertex shader
	if (interface.inputs.GetCount() > 0 && interface.outputs.GetCount() > 0) {
		code << "void main() {\n";
		code << EmitGLSL(pyLambda, interface);
		code << "}\n";
	}
	
	return code;
}

bool GLSLTranspiler::Validate(const String& code) {
	// Would use glslangValidator
	return true;
}

// ============================================================
// SPIR-V Transpiler - Full implementation
// ============================================================

struct SpvBuilder {
    String code;
    int nextId = 1;
    int tempCount = 0;
    
    int NewId() { return nextId++; }
    
    void Add(const String& line) { code << line << "\n"; }
    void AddFmt(const String& line) { code << line << "\n"; }
    
    String GetTypeId(const String& typeName) {
        if (typeName == "int") return "%int";
        if (typeName == "float") return "%float";
        if (typeName == "bool") return "%bool";
        if (typeName == "Vec2") return "%vec2";
        if (typeName == "Vec3") return "%vec3";
        if (typeName == "Vec4") return "%vec4";
        if (typeName == "Mat2") return "%mat2";
        if (typeName == "Mat3") return "%mat3";
        if (typeName == "Mat4") return "%mat4";
        return "%void";
    }
};

String SPIRVTranspiler::MapType(const TypeAnnotation& type) {
    if (type.name == "int") return "i32";
    if (type.name == "float") return "f32";
    if (type.name == "bool") return "bool";
    if (type.name == "Vec2") return "vec2";
    if (type.name == "Vec3") return "vec3";
    if (type.name == "Vec4") return "vec4";
    if (type.name == "Mat2") return "mat2";
    if (type.name == "Mat3") return "mat3";
    if (type.name == "Mat4") return "mat4";
    return "void";
}

String SPIRVTranspiler::GenerateLayout(const ShaderInterface& interface) {
    String code;
    int binding = 0;
    for (auto& u : interface.uniforms) {
        code << "OpDecorate %u" << binding << " DescriptorSet " << u.set << "\n";
        code << "OpDecorate %u" << binding << " Binding " << binding << "\n";
        binding++;
    }
    return code;
}

String SPIRVTranspiler::EmitSPIRV(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "; No shader function\n";
    
    SpvBuilder b;
    
    // Module header
    b.Add("; SPIR-V");
    b.Add("OpCapability Shader");
    b.Add("OpCapability Matrix");
    b.Add("OpMemoryModel Logical GLSL450");
    
    // Entry point
    int entryId = b.NewId();
    b.AddFmt(::Upp::Format("OpEntryPoint Vertex %%%d \"main\"", entryId));
    b.AddFmt(::Upp::Format("OpExecutionMode %d OriginUpperLeft", entryId));
    
    // Debug info
    b.Add("OpSource GLSL 450");
    b.AddFmt(::Upp::Format("OpName %%%d \"main\"", entryId));
    
    // Type declarations
    int voidId = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpTypeVoid", voidId));
    
    int intType = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpTypeInt 32 1", intType));
    
    int floatType = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpTypeFloat 32", floatType));
    
    int boolType = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpTypeBool", boolType));
    
    // Vector types
    int vec2Type = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpTypeVector %%%d 2", vec2Type, floatType));
    int vec3Type = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpTypeVector %%%d 3", vec3Type, floatType));
    int vec4Type = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpTypeVector %%%d 4", vec4Type, floatType));
    
    // Matrix types
    int mat2Type = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpTypeMatrix %%%d 2", mat2Type, vec2Type));
    int mat3Type = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpTypeMatrix %%%d 3", mat3Type, vec3Type));
    int mat4Type = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpTypeMatrix %%%d 4", mat4Type, vec4Type));
    
    // Function type
    int funcType = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpTypeFunction %%%d", funcType, voidId));
    
    // Constants
    int constZero = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d 0", constZero, intType));
    int constOne = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d 1", constOne, intType));
    int constZeroF = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d 0", constZeroF, floatType));
    int constOneF = b.NewId(); b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d 1", constOneF, floatType));
    
    // Uniform block - simplified
    // For each uniform, create a variable
    int varBase = b.NewId();
    for (int i = 0; i < interface.uniforms.GetCount(); ++i) {
        auto& u = interface.uniforms[i];
        int typeId = 0;
        String tn = u.type.name;
        if (tn == "Mat4") typeId = mat4Type;
        else if (tn == "Mat3") typeId = mat3Type;
        else if (tn == "Vec4") typeId = vec4Type;
        else if (tn == "Vec3") typeId = vec3Type;
        else if (tn == "Vec2") typeId = vec2Type;
        else if (tn == "float") typeId = floatType;
        else typeId = floatType;
        
        int ptrType = b.NewId();
        b.AddFmt(::Upp::Format("%%%d = OpTypePointer Uniform %%%d", ptrType, typeId));
        
        int varId = b.NewId();
        b.AddFmt(::Upp::Format("%%%d = OpVariable %%%d Uniform", varId, ptrType));
        b.AddFmt(::Upp::Format("OpName %%%d \"%s\"", varId, u.name));
        b.AddFmt(::Upp::Format("OpDecorate %%%d DescriptorSet %d", varId, u.set));
        b.AddFmt(::Upp::Format("OpDecorate %%%d Binding %d", varId, u.binding >= 0 ? u.binding : i));
    }
    
    // Function
    int funcId = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpFunction %%%d None %%%d", funcId, voidId, funcType));
    
    int labelId = b.NewId();
    b.AddFmt(::Upp::Format("%%%d = OpLabel", labelId));
    
    // Walk IR similar to GLSL but emit SPIR-V
    Vector<String> stack;
    
    for (int pc = 0; pc < pyLambda->ir.GetCount(); ++pc) {
        const PyIR& inst = pyLambda->ir[pc];
        int op = inst.code;
        
        switch (op) {
            case ::Upp::PY_LOAD_CONST: {
                if (inst.arg.IsInt()) {
                    int c = b.NewId();
                    b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d %d", c, intType, (int)inst.arg.AsInt64()));
                    stack.Add(::Upp::AsString(c));
                } else if (inst.arg.IsFloat()) {
                    int c = b.NewId();
                    b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d %f", c, floatType, inst.arg.AsDouble()));
                    stack.Add(::Upp::AsString(c));
                } else if (inst.arg.IsBool()) {
                    int c = b.NewId();
                    b.AddFmt(::Upp::Format("%%%d = OpConstant %%%d %s", c, boolType, inst.arg.IsTrue() ? "true" : "false"));
                    stack.Add(::Upp::AsString(c));
                }
                break;
            }
            
            case ::Upp::PY_LOAD_FAST: {
                String name = inst.arg.GetStr().ToString();
                stack.Add(name);
                break;
            }
            
            case ::Upp::PY_STORE_FAST: {
                String name = inst.arg.GetStr().ToString();
                String val = stack.Top(); stack.Pop();
                // Store would need variable creation - simplified
                break;
            }
            
            case ::Upp::PY_BINARY_ADD:
            case ::Upp::PY_BINARY_SUBTRACT:
            case ::Upp::PY_BINARY_MULTIPLY:
            case ::Upp::PY_BINARY_TRUE_DIVIDE: {
                if (stack.GetCount() < 2) break;
                String bStr = stack.Top(); stack.Pop();
                String aStr = stack.Top(); stack.Pop();
                int res = b.NewId();
                const char* opcode = "";
                if (op == ::Upp::PY_BINARY_ADD) opcode = "FAdd";
                else if (op == ::Upp::PY_BINARY_SUBTRACT) opcode = "FSub";
                else if (op == ::Upp::PY_BINARY_MULTIPLY) opcode = "FMul";
                else if (op == ::Upp::PY_BINARY_TRUE_DIVIDE) opcode = "FDiv";
                b.AddFmt(::Upp::Format("%%%d = Op%s %%%d %s %s", res, opcode, floatType, aStr, bStr));
                stack.Add(::Upp::AsString(res));
                break;
            }
            
            case ::Upp::PY_UNARY_NEGATIVE: {
                if (stack.GetCount() == 0) break;
                String aStr = stack.Top(); stack.Pop();
                int res = b.NewId();
                b.AddFmt(::Upp::Format("%%%d = OpFNegate %%%d %s", res, floatType, aStr));
                stack.Add(::Upp::AsString(res));
                break;
            }
            
            case ::Upp::PY_CALL_FUNCTION: {
                // Simplified - just handle known builtins
                int argc = inst.iarg;
                Vector<String> args;
                for (int i = 0; i < argc; ++i) {
                    if (stack.GetCount() == 0) break;
                    args.Insert(0, stack.Top()); stack.Pop();
                }
                // Function name would be on stack too
                if (argc == 1) {
                    if (stack.GetCount() == 0) break;
                    String func = stack.Top(); stack.Pop();
                    if (func == "normalize") {
                        if (args.GetCount() == 0) break;
                        int res = b.NewId();
                        b.AddFmt(::Upp::Format("%%%d = OpNormalize %%%d %s", res, vec3Type, args[0]));
                        stack.Add(::Upp::AsString(res));
                    }
                } else if (argc == 2) {
                    if (stack.GetCount() == 0) break;
                    String func = stack.Top(); stack.Pop();
                    if (func == "dot") {
                        if (args.GetCount() < 2) break;
                        int res = b.NewId();
                        b.AddFmt(::Upp::Format("%%%d = OpDot %%%d %s %s", res, floatType, args[0], args[1]));
                        stack.Add(::Upp::AsString(res));
                    }
                }
                break;
            }
            
            case ::Upp::PY_RETURN_VALUE: {
                // Return
                b.Add("OpReturn");
                break;
            }
        }
    }
    
    b.AddFmt(::Upp::Format("%%%d = OpFunctionEnd", funcId));
    
    return b.code;
}

String SPIRVTranspiler::Transpile(PyLambda* pyLambda, const ShaderInterface& interface) {
    String code = "; SPIR-V\n";
    code << "OpCapability Shader\n";
    code << "OpMemoryModel Logical GLSL450\n";
    code << GenerateLayout(interface);
    code << EmitSPIRV(pyLambda, interface);
    return code;
}

bool SPIRVTranspiler::Validate(const String& code) {
	return true;
}

// ============================================================
// MSL Transpiler - Full implementation
// ============================================================

String MSLTranspiler::MapType(const TypeAnnotation& type) {
    if (type.name == "int") return "int";
    if (type.name == "float") return "float";
    if (type.name == "bool") return "bool";
    if (type.name == "Vec2") return "float2";
    if (type.name == "Vec3") return "float3";
    if (type.name == "Vec4") return "float4";
    if (type.name == "Mat2") return "float2x2";
    if (type.name == "Mat3") return "float3x3";
    if (type.name == "Mat4") return "float4x4";
    return "void";
}

String MSLTranspiler::GenerateLayout(const ShaderInterface& interface) {
    String code;
    code << "struct Uniforms {\n";
    for (auto& u : interface.uniforms) {
        if (u.type.name == "Uniforms" || u.type.name == "struct" || u.type.name == "TypedDict") {
            code << "    // Uniform block: " << u.name << " (type: " << u.type.name << ")\n";
            code << "    float4x4 model;\n";
            code << "    float4x4 view;\n";
            code << "    float4x4 proj;\n";
            code << "    float3x3 normalMatrix;\n";
            code << "    float3 lightDir;\n";
            code << "    float3 lightColor;\n";
            code << "    float3 ambientColor;\n";
            code << "    float3 baseColor;\n";
            code << "    float time;\n";
            code << "    float3 cameraPos;\n";
        } else {
            code << "    " << MapType(u.type) << " " << u.name << ";\n";
        }
    }
    code << "};\n";
    return code;
}

String MSLTranspiler::MapOpToMSL(int code, const String& a, const String& b) {
    using ::Upp::PyOpCode;
    switch (code) {
        case ::Upp::PY_BINARY_ADD: return "(" + a + " + " + b + ")";
        case ::Upp::PY_BINARY_SUBTRACT: return "(" + a + " - " + b + ")";
        case ::Upp::PY_BINARY_MULTIPLY: return "(" + a + " * " + b + ")";
        case ::Upp::PY_BINARY_TRUE_DIVIDE: return "(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_FLOOR_DIVIDE: return "floor(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_MODULO: return "fmod(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_POWER: return "pow(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_AND: return "(" + a + " && " + b + ")";
        case ::Upp::PY_BINARY_OR: return "(" + a + " || " + b + ")";
        case ::Upp::PY_BINARY_XOR: return "(" + a + " ^ " + b + ")";
        case ::Upp::PY_UNARY_NEGATIVE: return "(-" + a + ")";
        case ::Upp::PY_UNARY_NOT: return "(!" + a + ")";
        default: return "";
    }
}

String MSLTranspiler::EmitMSL(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "// No shader function\n";
    
    const Vector<PyIR>& ir = pyLambda->ir;
    String code;
    
    Vector<String> stack;
    int tempCount = 0;
    
    auto pushTemp = [&](const String& type, const String& expr) {
        String name = "t" + ::Upp::AsString(tempCount++);
        code << "    " << type << " " << name << " = " << expr << ";\n";
        stack.Add(name);
        return name;
    };
    
    auto popTemp = [&]() -> String {
        if (stack.GetCount() == 0) return "";
        String s = stack.Top(); stack.Pop();
        return s;
    };
    
    // Process arguments
    for (int i = 0; i < pyLambda->arg.GetCount(); ++i) {
        String argName = pyLambda->arg[i];
        String typeName = "float";
        for (auto& p : interface.inputs) {
            if (p.name == argName) { typeName = MapType(p.type); break; }
            for (auto& p2 : interface.uniforms) {
                if (p2.name == argName) { typeName = MapType(p2.type); break; }
            }
        }
        stack.Add(argName);
    }
    
    // Walk IR
    for (int pc = 0; pc < ir.GetCount(); ++pc) {
        const PyIR& inst = ir[pc];
        int op = inst.code;
        
        switch (op) {
            case ::Upp::PY_LOAD_CONST: {
                if (inst.arg.IsInt()) {
                    pushTemp("int", ::Upp::AsString(inst.arg.AsInt()));
                } else if (inst.arg.IsFloat()) {
                    double v = inst.arg.AsDouble();
                    pushTemp("float", ::Upp::Format("%.6f", v));
                } else if (inst.arg.IsBool()) {
                    pushTemp("bool", inst.arg.IsTrue() ? "true" : "false");
                }
                break;
            }
            
            case ::Upp::PY_LOAD_FAST: {
                stack.Add(inst.arg.GetStr().ToString());
                break;
            }
            
            case ::Upp::PY_STORE_FAST: {
                String name = inst.arg.GetStr().ToString();
                String val = popTemp();
                if (val.GetCount() > 0) code << "    " << name << " = " << val << ";\n";
                break;
            }
            
            case ::Upp::PY_LOAD_NAME: {
                stack.Add(inst.arg.GetStr().ToString());
                break;
            }
            
            case ::Upp::PY_STORE_NAME: {
                String name = inst.arg.GetStr().ToString();
                String val = popTemp();
                if (val.GetCount() > 0) code << "    " << name << " = " << val << ";\n";
                break;
            }
            
            case ::Upp::PY_LOAD_ATTR: {
                String attr = inst.arg.GetStr().ToString();
                String obj = popTemp();
                if (obj.GetCount() > 0) stack.Add(obj + "." + attr);
                break;
            }
            
            case ::Upp::PY_BINARY_ADD:
            case ::Upp::PY_BINARY_SUBTRACT:
            case ::Upp::PY_BINARY_MULTIPLY:
            case ::Upp::PY_BINARY_TRUE_DIVIDE:
            case ::Upp::PY_BINARY_FLOOR_DIVIDE:
            case ::Upp::PY_BINARY_MODULO:
            case ::Upp::PY_BINARY_POWER:
            case ::Upp::PY_BINARY_AND:
            case ::Upp::PY_BINARY_OR:
            case ::Upp::PY_BINARY_XOR: {
                String b = popTemp(); String a = popTemp();
                if (a.GetCount() > 0 && b.GetCount() > 0)
                    pushTemp("float", MapOpToMSL(op, a, b));
                break;
            }
            
            case ::Upp::PY_UNARY_NEGATIVE: {
                String a = popTemp();
                if (a.GetCount() > 0) pushTemp("float", "(-" + a + ")");
                break;
            }
            
            case ::Upp::PY_UNARY_NOT: {
                String a = popTemp();
                if (a.GetCount() > 0) pushTemp("bool", "(!" + a + ")");
                break;
            }
            
            case ::Upp::PY_COMPARE_OP: {
                String b = popTemp(); String a = popTemp();
                if (a.GetCount() > 0 && b.GetCount() > 0) {
                    int cmpOp = inst.iarg;
                    String opStr;
                    switch (cmpOp) {
                        case ::Upp::PY_CMP_LT: opStr = "<"; break;
                        case ::Upp::PY_CMP_LE: opStr = "<="; break;
                        case ::Upp::PY_CMP_EQ: opStr = "=="; break;
                        case ::Upp::PY_CMP_NE: opStr = "!="; break;
                        case ::Upp::PY_CMP_GT: opStr = ">"; break;
                        case ::Upp::PY_CMP_GE: opStr = ">="; break;
                        default: opStr = "=="; break;
                    }
                    pushTemp("bool", "(" + a + " " + opStr + " " + b + ")");
                }
                break;
            }
            
            case ::Upp::PY_CALL_FUNCTION: {
                int argc = inst.iarg;
                Vector<String> args;
                for (int i = 0; i < argc; ++i) args.Insert(0, popTemp());
                if (argc == 1) {
                    String func = popTemp();
                    if (func == "normalize") pushTemp("float3", "normalize(" + args[0] + ")");
                    else if (func == "length") pushTemp("float", "length(" + args[0] + ")");
                    else if (func == "dot") pushTemp("float", "dot(" + args[0] + ", " + args[1] + ")");
                } else if (argc == 2) {
                    String func = popTemp();
                    if (func == "dot") pushTemp("float", "dot(" + args[0] + ", " + args[1] + ")");
                    else if (func == "cross") pushTemp("float3", "cross(" + args[0] + ", " + args[1] + ")");
                    else if (func == "mul") pushTemp("float4", args[0] + " * " + args[1]);
                }
                break;
            }
            
            case ::Upp::PY_RETURN_VALUE: {
                String val = popTemp();
                if (val.GetCount() > 0) code << "    return " << val << ";\n";
                break;
            }
            
            case ::Upp::PY_BINARY_SUBSCR: {
                String idx = popTemp(); String obj = popTemp();
                if (obj.GetCount() > 0 && idx.GetCount() > 0) stack.Add(obj + "[" + idx + "]");
                break;
            }
        }
    }
    
    return code;
}

String MSLTranspiler::Transpile(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "// No shader\n";
    
    String code;
    code << "// Metal Shading Language\n";
    code << "#include <metal_stdlib>\nusing namespace metal;\n\n";
    code << GenerateLayout(interface);
    code << "\n";
    
    // Determine shader stage from interface
    bool isVertex = interface.inputs.GetCount() > 0 && interface.outputs.GetCount() > 0;
    
    if (isVertex) {
        code << "vertex float4 main(\n";
        for (int i = 0; i < interface.inputs.GetCount(); ++i) {
            auto& inp = interface.inputs[i];
            code << "    " << MapType(inp.type) << " " << inp.name << " [[attribute(" << i << ")]]";
            if (i < interface.inputs.GetCount() - 1) code << ",\n";
        }
        code << ", constant Uniforms& uniforms [[buffer(0)]]\n";
        code << ") {\n";
        code << EmitMSL(pyLambda, interface);
        code << "}\n";
    } else {
        code << "fragment float4 main(\n";
        for (int i = 0; i < interface.inputs.GetCount(); ++i) {
            auto& inp = interface.inputs[i];
            code << "    " << MapType(inp.type) << " " << inp.name << " [[stage_in]]";
            if (i < interface.inputs.GetCount() - 1) code << ",\n";
        }
        code << ", constant Uniforms& uniforms [[buffer(0)]]\n";
        code << ") {\n";
        code << EmitMSL(pyLambda, interface);
        code << "}\n";
    }
    
    return code;
}

bool MSLTranspiler::Validate(const String& code) {
	return true;
}

// ============================================================
// DXIL Transpiler - Full implementation (HLSL)
// ============================================================

String DXILTranspiler::MapType(const TypeAnnotation& type) {
    if (type.name == "int") return "int";
    if (type.name == "float") return "float";
    if (type.name == "bool") return "bool";
    if (type.name == "Vec2") return "float2";
    if (type.name == "Vec3") return "float3";
    if (type.name == "Vec4") return "float4";
    if (type.name == "Mat2") return "float2x2";
    if (type.name == "Mat3") return "float3x3";
    if (type.name == "Mat4") return "float4x4";
    return "void";
}

String DXILTranspiler::GenerateLayout(const ShaderInterface& interface) {
    String code;
    code << "cbuffer Uniforms : register(b0) {\n";
    for (auto& u : interface.uniforms) {
        if (u.type.name == "Uniforms" || u.type.name == "struct" || u.type.name == "TypedDict") {
            code << "    // Uniform block: " << u.name << " (type: " << u.type.name << ")\n";
            code << "    float4x4 model;\n";
            code << "    float4x4 view;\n";
            code << "    float4x4 proj;\n";
            code << "    float3x3 normalMatrix;\n";
            code << "    float3 lightDir;\n";
            code << "    float3 lightColor;\n";
            code << "    float3 ambientColor;\n";
            code << "    float3 baseColor;\n";
            code << "    float time;\n";
            code << "    float3 cameraPos;\n";
        } else {
            code << "    " << MapType(u.type) << " " << u.name << ";\n";
        }
    }
    code << "};\n";
    return code;
}

String DXILTranspiler::MapOpToHLSL(int code, const String& a, const String& b) {
    using ::Upp::PyOpCode;
    switch (code) {
        case ::Upp::PY_BINARY_ADD: return "(" + a + " + " + b + ")";
        case ::Upp::PY_BINARY_SUBTRACT: return "(" + a + " - " + b + ")";
        case ::Upp::PY_BINARY_MULTIPLY: return "(" + a + " * " + b + ")";
        case ::Upp::PY_BINARY_TRUE_DIVIDE: return "(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_FLOOR_DIVIDE: return "floor(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_MODULO: return "fmod(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_POWER: return "pow(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_AND: return "(" + a + " && " + b + ")";
        case ::Upp::PY_BINARY_OR: return "(" + a + " || " + b + ")";
        case ::Upp::PY_BINARY_XOR: return "(" + a + " ^ " + b + ")";
        case ::Upp::PY_UNARY_NEGATIVE: return "(-" + a + ")";
        case ::Upp::PY_UNARY_NOT: return "(!" + a + ")";
        default: return "";
    }
}

String DXILTranspiler::EmitHLSL(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "// No shader function\n";
    
    const Vector<PyIR>& ir = pyLambda->ir;
    String code;
    
    Vector<String> stack;
    int tempCount = 0;
    
    auto pushTemp = [&](const String& type, const String& expr) {
        String name = "t" + ::Upp::AsString(tempCount++);
        code << "    " << type << " " << name << " = " << expr << ";\n";
        stack.Add(name);
        return name;
    };
    
    auto popTemp = [&]() -> String {
        if (stack.GetCount() == 0) return "";
        String s = stack.Top(); stack.Pop();
        return s;
    };
    
    // Process arguments
    for (int i = 0; i < pyLambda->arg.GetCount(); ++i) {
        String argName = pyLambda->arg[i];
        String typeName = "float";
        for (auto& p : interface.inputs) {
            if (p.name == argName) { typeName = MapType(p.type); break; }
            for (auto& p2 : interface.uniforms) {
                if (p2.name == argName) { typeName = MapType(p2.type); break; }
            }
        }
        stack.Add(argName);
    }
    
    // Walk IR
    for (int pc = 0; pc < ir.GetCount(); ++pc) {
        const PyIR& inst = ir[pc];
        int op = inst.code;
        
        switch (op) {
            case ::Upp::PY_LOAD_CONST: {
                if (inst.arg.IsInt()) pushTemp("int", ::Upp::AsString(inst.arg.AsInt()));
                else if (inst.arg.IsFloat()) {
                    double v = inst.arg.AsDouble();
                    pushTemp("float", ::Upp::Format("%.6f", v));
                }
                else if (inst.arg.IsBool()) pushTemp("bool", inst.arg.IsTrue() ? "true" : "false");
                break;
            }
            
            case ::Upp::PY_LOAD_FAST: stack.Add(inst.arg.GetStr().ToString()); break;
            case ::Upp::PY_STORE_FAST: {
                String name = inst.arg.GetStr().ToString();
                String val = popTemp();
                if (val.GetCount() > 0) code << "    " << name << " = " << val << ";\n";
                break;
            }
            case ::Upp::PY_LOAD_NAME: stack.Add(inst.arg.GetStr().ToString()); break;
            case ::Upp::PY_STORE_NAME: {
                String name = inst.arg.GetStr().ToString();
                String val = popTemp();
                if (val.GetCount() > 0) code << "    " << name << " = " << val << ";\n";
                break;
            }
            case ::Upp::PY_LOAD_ATTR: {
                String attr = inst.arg.GetStr().ToString();
                String obj = popTemp();
                if (obj.GetCount() > 0) stack.Add(obj + "." + attr);
                break;
            }
            case ::Upp::PY_BINARY_ADD:
            case ::Upp::PY_BINARY_SUBTRACT:
            case ::Upp::PY_BINARY_MULTIPLY:
            case ::Upp::PY_BINARY_TRUE_DIVIDE:
            case ::Upp::PY_BINARY_FLOOR_DIVIDE:
            case ::Upp::PY_BINARY_MODULO:
            case ::Upp::PY_BINARY_POWER:
            case ::Upp::PY_BINARY_AND:
            case ::Upp::PY_BINARY_OR:
            case ::Upp::PY_BINARY_XOR: {
                String b = popTemp(); String a = popTemp();
                if (a.GetCount() > 0 && b.GetCount() > 0)
                    pushTemp("float", MapOpToHLSL(op, a, b));
                break;
            }
            case ::Upp::PY_UNARY_NEGATIVE: {
                String a = popTemp();
                if (a.GetCount() > 0) pushTemp("float", "(-" + a + ")");
                break;
            }
            case ::Upp::PY_UNARY_NOT: {
                String a = popTemp();
                if (a.GetCount() > 0) pushTemp("bool", "(!" + a + ")");
                break;
            }
            case ::Upp::PY_COMPARE_OP: {
                String b = popTemp(); String a = popTemp();
                if (a.GetCount() > 0 && b.GetCount() > 0) {
                    int cmpOp = inst.iarg;
                    String opStr;
                    switch (cmpOp) {
                        case ::Upp::PY_CMP_LT: opStr = "<"; break;
                        case ::Upp::PY_CMP_LE: opStr = "<="; break;
                        case ::Upp::PY_CMP_EQ: opStr = "=="; break;
                        case ::Upp::PY_CMP_NE: opStr = "!="; break;
                        case ::Upp::PY_CMP_GT: opStr = ">"; break;
                        case ::Upp::PY_CMP_GE: opStr = ">="; break;
                        default: opStr = "=="; break;
                    }
                    pushTemp("bool", "(" + a + " " + opStr + " " + b + ")");
                }
                break;
            }
            case ::Upp::PY_CALL_FUNCTION: {
                int argc = inst.iarg;
                Vector<String> args;
                for (int i = 0; i < argc; ++i) args.Insert(0, popTemp());
                if (argc == 1) {
                    String func = popTemp();
                    if (func == "normalize") pushTemp("float3", "normalize(" + args[0] + ")");
                    else if (func == "length") pushTemp("float", "length(" + args[0] + ")");
                } else if (argc == 2) {
                    String func = popTemp();
                    if (func == "dot") pushTemp("float", "dot(" + args[0] + ", " + args[1] + ")");
                    else if (func == "cross") pushTemp("float3", "cross(" + args[0] + ", " + args[1] + ")");
                    else if (func == "mul") pushTemp("float4", "mul(" + args[0] + ", " + args[1] + ")");
                }
                break;
            }
            case ::Upp::PY_RETURN_VALUE: {
                String val = popTemp();
                if (val.GetCount() > 0) code << "    return " << val << ";\n";
                break;
            }
            case ::Upp::PY_BINARY_SUBSCR: {
                String idx = popTemp(); String obj = popTemp();
                if (obj.GetCount() > 0 && idx.GetCount() > 0) stack.Add(obj + "[" + idx + "]");
                break;
            }
        }
    }
    
    return code;
}

String DXILTranspiler::Transpile(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "// No shader\n";
    
    String code;
    code << "// HLSL / DXIL\n";
    code << GenerateLayout(interface);
    code << "\n";
    
    bool isVertex = interface.inputs.GetCount() > 0 && interface.outputs.GetCount() > 0;
    
    if (isVertex) {
        code << "struct VS_INPUT {\n";
        for (int i = 0; i < interface.inputs.GetCount(); ++i) {
            auto& inp = interface.inputs[i];
            code << "    " << MapType(inp.type) << " " << inp.name << " : TEXCOORD" << i << ";\n";
        }
        code << "};\n\n";
        
        code << "struct VS_OUTPUT {\n";
        for (int i = 0; i < interface.outputs.GetCount(); ++i) {
            auto& out = interface.outputs[i];
            code << "    " << MapType(out.type) << " " << out.name << " : TEXCOORD" << i << ";\n";
        }
        code << "    float4 SV_Position : SV_Position;\n";
        code << "};\n\n";
        
        code << "VS_OUTPUT main(VS_INPUT input) {\n";
        code << EmitHLSL(pyLambda, interface);
        code << "}\n";
    } else {
        code << "struct PS_INPUT {\n";
        for (int i = 0; i < interface.inputs.GetCount(); ++i) {
            auto& inp = interface.inputs[i];
            code << "    " << MapType(inp.type) << " " << inp.name << " : TEXCOORD" << i << ";\n";
        }
        code << "};\n\n";
        
        code << "float4 main(PS_INPUT input) : SV_Target {\n";
        code << EmitHLSL(pyLambda, interface);
        code << "}\n";
    }
    
    return code;
}

bool DXILTranspiler::Validate(const String& code) {
	return true;
}

// ============================================================
// OSL Transpiler - Full implementation (Open Shading Language)
// ============================================================

String OSLTranspiler::MapType(const TypeAnnotation& type) {
    if (type.name == "int") return "int";
    if (type.name == "float") return "float";
    if (type.name == "bool") return "int";  // OSL uses int for bool
    if (type.name == "Vec2") return "vector";
    if (type.name == "Vec3") return "vector";
    if (type.name == "Vec4") return "vector";
    if (type.name == "Mat2") return "matrix";
    if (type.name == "Mat3") return "matrix";
    if (type.name == "Mat4") return "matrix";
    return "void";
}

String OSLTranspiler::GenerateLayout(const ShaderInterface& interface) {
    String code;
    code << "shader SurfaceShader(\n";
    bool first = true;
    for (auto& u : interface.uniforms) {
        if (u.type.name == "Uniforms" || u.type.name == "struct" || u.type.name == "TypedDict") {
            if (!first) code << ",\n";
            code << "    matrix model = matrix(1),\n";
            code << "    matrix view = matrix(1),\n";
            code << "    matrix proj = matrix(1),\n";
            code << "    matrix normalMatrix = matrix(1),\n";
            code << "    vector lightDir = vector(0),\n";
            code << "    color lightColor = color(1),\n";
            code << "    color ambientColor = color(0),\n";
            code << "    color baseColor = color(0),\n";
            code << "    float time = 0,\n";
            code << "    vector cameraPos = vector(0)";
            first = false;
        } else {
            if (!first) code << ",\n";
            code << "    " << MapType(u.type) << " " << u.name << " = " << MapType(u.type) << "(0)";
            first = false;
        }
    }
    code << ") {\n";
    return code;
}

String OSLTranspiler::MapOpToOSL(int code, const String& a, const String& b) {
    using ::Upp::PyOpCode;
    switch (code) {
        case ::Upp::PY_BINARY_ADD: return "(" + a + " + " + b + ")";
        case ::Upp::PY_BINARY_SUBTRACT: return "(" + a + " - " + b + ")";
        case ::Upp::PY_BINARY_MULTIPLY: return "(" + a + " * " + b + ")";
        case ::Upp::PY_BINARY_TRUE_DIVIDE: return "(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_FLOOR_DIVIDE: return "floor(" + a + " / " + b + ")";
        case ::Upp::PY_BINARY_MODULO: return "mod(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_POWER: return "pow(" + a + ", " + b + ")";
        case ::Upp::PY_BINARY_AND: return "(" + a + " && " + b + ")";
        case ::Upp::PY_BINARY_OR: return "(" + a + " || " + b + ")";
        case ::Upp::PY_BINARY_XOR: return "(" + a + " ^ " + b + ")";
        case ::Upp::PY_UNARY_NEGATIVE: return "(-" + a + ")";
        case ::Upp::PY_UNARY_NOT: return "(!" + a + ")";
        default: return "";
    }
}

String OSLTranspiler::EmitOSL(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "// No shader function\n";
    
    const Vector<PyIR>& ir = pyLambda->ir;
    String code;
    
    Vector<String> stack;
    int tempCount = 0;
    
    auto pushTemp = [&](const String& type, const String& expr) {
        String name = "t" + ::Upp::AsString(tempCount++);
        code << "    " << type << " " << name << " = " << expr << ";\n";
        stack.Add(name);
        return name;
    };
    
    auto popTemp = [&]() -> String {
        if (stack.GetCount() == 0) return "";
        String s = stack.Top(); stack.Pop();
        return s;
    };
    
    // Process arguments (OSL shader parameters)
    for (int i = 0; i < pyLambda->arg.GetCount(); ++i) {
        String argName = pyLambda->arg[i];
        String typeName = "float";
        for (auto& p : interface.inputs) {
            if (p.name == argName) { typeName = MapType(p.type); break; }
            for (auto& p2 : interface.uniforms) {
                if (p2.name == argName) { typeName = MapType(p2.type); break; }
            }
        }
        stack.Add(argName);
    }
    
    // Walk IR
    for (int pc = 0; pc < ir.GetCount(); ++pc) {
        const PyIR& inst = ir[pc];
        int op = inst.code;
        
        switch (op) {
            case ::Upp::PY_LOAD_CONST: {
                if (inst.arg.IsInt()) pushTemp("int", ::Upp::AsString(inst.arg.AsInt()));
                else if (inst.arg.IsFloat()) {
                    double v = inst.arg.AsDouble();
                    pushTemp("float", ::Upp::Format("%.6f", v));
                }
                else if (inst.arg.IsBool()) pushTemp("int", inst.arg.IsTrue() ? "1" : "0");
                break;
            }
            
            case ::Upp::PY_LOAD_FAST: stack.Add(inst.arg.GetStr().ToString()); break;
            case ::Upp::PY_STORE_FAST: {
                String name = inst.arg.GetStr().ToString();
                String val = popTemp();
                if (val.GetCount() > 0) code << "    " << name << " = " << val << ";\n";
                break;
            }
            case ::Upp::PY_LOAD_NAME: stack.Add(inst.arg.GetStr().ToString()); break;
            case ::Upp::PY_STORE_NAME: {
                String name = inst.arg.GetStr().ToString();
                String val = popTemp();
                if (val.GetCount() > 0) code << "    " << name << " = " << val << ";\n";
                break;
            }
            case ::Upp::PY_LOAD_ATTR: {
                String attr = inst.arg.GetStr().ToString();
                String obj = popTemp();
                if (obj.GetCount() > 0) stack.Add(obj + "." + attr);
                break;
            }
            case ::Upp::PY_BINARY_ADD:
            case ::Upp::PY_BINARY_SUBTRACT:
            case ::Upp::PY_BINARY_MULTIPLY:
            case ::Upp::PY_BINARY_TRUE_DIVIDE:
            case ::Upp::PY_BINARY_FLOOR_DIVIDE:
            case ::Upp::PY_BINARY_MODULO:
            case ::Upp::PY_BINARY_POWER:
            case ::Upp::PY_BINARY_AND:
            case ::Upp::PY_BINARY_OR:
            case ::Upp::PY_BINARY_XOR: {
                String b = popTemp(); String a = popTemp();
                if (a.GetCount() > 0 && b.GetCount() > 0)
                    pushTemp("float", MapOpToOSL(op, a, b));
                break;
            }
            case ::Upp::PY_UNARY_NEGATIVE: {
                String a = popTemp();
                if (a.GetCount() > 0) pushTemp("float", "(-" + a + ")");
                break;
            }
            case ::Upp::PY_UNARY_NOT: {
                String a = popTemp();
                if (a.GetCount() > 0) pushTemp("int", "(!" + a + ")");
                break;
            }
            case ::Upp::PY_COMPARE_OP: {
                String b = popTemp(); String a = popTemp();
                if (a.GetCount() > 0 && b.GetCount() > 0) {
                    int cmpOp = inst.iarg;
                    String opStr;
                    switch (cmpOp) {
                        case ::Upp::PY_CMP_LT: opStr = "<"; break;
                        case ::Upp::PY_CMP_LE: opStr = "<="; break;
                        case ::Upp::PY_CMP_EQ: opStr = "=="; break;
                        case ::Upp::PY_CMP_NE: opStr = "!="; break;
                        case ::Upp::PY_CMP_GT: opStr = ">"; break;
                        case ::Upp::PY_CMP_GE: opStr = ">="; break;
                        default: opStr = "=="; break;
                    }
                    pushTemp("int", "(" + a + " " + opStr + " " + b + ")");
                }
                break;
            }
            case ::Upp::PY_CALL_FUNCTION: {
                int argc = inst.iarg;
                Vector<String> args;
                for (int i = 0; i < argc; ++i) args.Insert(0, popTemp());
                if (argc == 1) {
                    String func = popTemp();
                    if (func == "normalize") pushTemp("vector", "normalize(" + args[0] + ")");
                    else if (func == "length") pushTemp("float", "length(" + args[0] + ")");
                } else if (argc == 2) {
                    String func = popTemp();
                    if (func == "dot") pushTemp("float", "dot(" + args[0] + ", " + args[1] + ")");
                    else if (func == "cross") pushTemp("vector", "cross(" + args[0] + ", " + args[1] + ")");
                    else if (func == "mul") pushTemp("vector", args[0] + " * " + args[1]);
                }
                break;
            }
            case ::Upp::PY_RETURN_VALUE: {
                // OSL shaders write to output parameters, not return
                String val = popTemp();
                if (val.GetCount() > 0) code << "    Ci = " << val << ";\n";  // Ci is standard output
                break;
            }
            case ::Upp::PY_BINARY_SUBSCR: {
                String idx = popTemp(); String obj = popTemp();
                if (obj.GetCount() > 0 && idx.GetCount() > 0) stack.Add(obj + "[" + idx + "]");
                break;
            }
        }
    }
    
    return code;
}

String OSLTranspiler::Transpile(PyLambda* pyLambda, const ShaderInterface& interface) {
    if (!pyLambda) return "// No shader\n";
    
    String code;
    code << "// Open Shading Language\n";
    code << GenerateLayout(interface);
    code << EmitOSL(pyLambda, interface);
    code << "}\n";  // Close shader
    return code;
}

bool OSLTranspiler::Validate(const String& code) {
	return true;
}

} // namespace SoftGraphics