#include "CpuBackend.h"
#include "Shader.h"
#include "Texture.h"
#include <ByteVM/Python/PyVM.h>
#include <ByteVM/Python/PyCode.h>
#include <Geometry/Geometry.h>
#include <math.h>

namespace SoftGraphics {

using Upp::PyValue;
using Upp::PyVM;
using Upp::Vector;
using Upp::VectorMap;
using Upp::PyUserData;
using Upp::byte;
using Upp::vec2;
using Upp::vec3;
using Upp::vec4;

// ---------------------------------------------------------------------------
// Shader value types (typed-Python userdata exposed to the interpreter)
// ---------------------------------------------------------------------------

struct PyVec2 : PyUserData {
	vec2 v;
	PyVec2() : v(0, 0) {}
	PyVec2(double x, double y) : v((float)x, (float)y) {}
	String GetTypeName() const override { return "Vec2"; }
	PyValue GetAttr(const String& attr) override {
		if(attr == "x") return PyValue((double)v[0]);
		if(attr == "y") return PyValue((double)v[1]);
		return PyValue::None();
	}
};

struct PyVec3 : PyUserData {
	vec3 v;
	PyVec3() : v(0, 0, 0) {}
	PyVec3(double x, double y, double z) : v((float)x, (float)y, (float)z) {}
	String GetTypeName() const override { return "Vec3"; }
	PyValue GetAttr(const String& attr) override {
		if(attr == "x") return PyValue((double)v[0]);
		if(attr == "y") return PyValue((double)v[1]);
		if(attr == "z") return PyValue((double)v[2]);
		return PyValue::None();
	}
};

struct PyVec4 : PyUserData {
	vec4 v;
	PyVec4() : v(0, 0, 0, 0) {}
	PyVec4(double x, double y, double z, double w) : v((float)x, (float)y, (float)z, (float)w) {}
	String GetTypeName() const override { return "Vec4"; }
	PyValue GetAttr(const String& attr) override {
		if(attr == "x") return PyValue((double)v[0]);
		if(attr == "y") return PyValue((double)v[1]);
		if(attr == "z") return PyValue((double)v[2]);
		if(attr == "w") return PyValue((double)v[3]);
		return PyValue::None();
	}
};

struct PyMat4 : PyUserData {
	mat4 m;
	PyMat4() { m.SetIdentity(); }
	String GetTypeName() const override { return "Mat4"; }
};

// Flatten a value into a component list, GLSL-constructor style: a Vec2/3/4
// contributes its components, a list/tuple contributes its elements, a scalar
// contributes one component.
static void PushComponents(const PyValue& value, Vector<double>& out) {
	if(value.IsUserData()) {
		PyUserData& ud = value.GetUserData();
		String tn = ud.GetTypeName();
		if(tn == "Vec2") {
			PyVec2& p = static_cast<PyVec2&>(ud);
			out.Add(p.v[0]); out.Add(p.v[1]);
		}
		else if(tn == "Vec3") {
			PyVec3& p = static_cast<PyVec3&>(ud);
			out.Add(p.v[0]); out.Add(p.v[1]); out.Add(p.v[2]);
		}
		else if(tn == "Vec4") {
			PyVec4& p = static_cast<PyVec4&>(ud);
			out.Add(p.v[0]); out.Add(p.v[1]); out.Add(p.v[2]); out.Add(p.v[3]);
		}
		return;
	}
	int t = value.GetType();
	if(t == ::Upp::PY_TUPLE || t == ::Upp::PY_LIST) {
		for(const PyValue& e : value.GetArray())
			PushComponents(e, out);
		return;
	}
	if(value.IsNumber())
		out.Add(value.AsDouble());
}

// GLSL-style constructor: gather components from all arguments and fill an
// N-component vector (missing components default to 0, w defaults to 1).
static PyValue MakeVec(const Vector<PyValue>& args, int n) {
	Vector<double> c;
	for(const PyValue& a : args)
		PushComponents(a, c);
	double comp[4] = {0, 0, 0, 1};
	for(int i = 0; i < n; i++)
		comp[i] = i < c.GetCount() ? c[i] : (i == 3 ? 1.0 : 0.0);
	switch(n) {
		case 2:  return PyValue::UserData(new PyVec2(comp[0], comp[1]));
		case 3:  return PyValue::UserData(new PyVec3(comp[0], comp[1], comp[2]));
		default: return PyValue::UserData(new PyVec4(comp[0], comp[1], comp[2], comp[3]));
	}
}

// Convert any shader value to a vec4 (Vec2 -> z=0,w=1; Vec3 -> w=1; scalar -> (s,0,0,1)).
static bool ToVec4(const PyValue& value, vec4& out) {
	Vector<double> c;
	PushComponents(value, c);
	if(c.IsEmpty()) return false;
	out = vec4(c.GetCount() > 0 ? (float)c[0] : 0.0f,
	           c.GetCount() > 1 ? (float)c[1] : 0.0f,
	           c.GetCount() > 2 ? (float)c[2] : 0.0f,
	           c.GetCount() > 3 ? (float)c[3] : 1.0f);
	return true;
}

static PyValue BuiltinVoid(const Vector<PyValue>& args, void* user_data) {
	(void)args; (void)user_data;
	return PyValue::None();
}

// Register execution-time constructors and builtins into a specific globals
// dict. Called on the compiled shader's own globals because a PyLambda resolves
// names against its captured `globals`, not against a fresh execution VM.
void RegisterCpuBackendBuiltins(PyValue& globals) {
	if(globals.GetType() != ::Upp::PY_DICT) return;
	VectorMap<PyValue, PyValue>& dict = globals.GetDictRW();

	auto reg = [&](const char* name, PyValue (*builtin)(const Vector<PyValue>&, void*)) {
		PyValue f = PyValue::Function(name, builtin, nullptr);
		int q = dict.Find(PyValue(name));
		if(q >= 0) dict[q] = f;
		else dict.Add(PyValue(name), f);
	};

	// Math type constructors (override the compile-time subscriptable typing forms)
	reg("Vec2", [](const Vector<PyValue>& a, void*) { return MakeVec(a, 2); });
	reg("Vec3", [](const Vector<PyValue>& a, void*) { return MakeVec(a, 3); });
	reg("Vec4", [](const Vector<PyValue>& a, void*) { return MakeVec(a, 4); });
	reg("Mat2", [](const Vector<PyValue>&, void*) { return PyValue::UserData(new PyMat4()); });
	reg("Mat3", [](const Vector<PyValue>&, void*) { return PyValue::UserData(new PyMat4()); });
	reg("Mat4", [](const Vector<PyValue>&, void*) { return PyValue::UserData(new PyMat4()); });
	reg("IVec2", [](const Vector<PyValue>& a, void*) { return MakeVec(a, 2); });
	reg("IVec3", [](const Vector<PyValue>& a, void*) { return MakeVec(a, 3); });
	reg("IVec4", [](const Vector<PyValue>& a, void*) { return MakeVec(a, 4); });
	reg("void", BuiltinVoid);

	// Sampler / buffer / image forms are opaque placeholders for CPU validation.
	static const char* const opaque[] = {
		"Sampler1D", "Sampler2D", "Sampler3D", "SamplerCube", "Sampler2DArray",
		"StorageBuffer", "UniformBuffer", "PushConstant", "StorageImage",
	};
	for(const char* name : opaque)
		reg(name, BuiltinVoid);

	// GLSL/HLSL builtin functions: registered so shader bodies type-check and
	// resolve at execution. Numeric ones are not evaluated by the CPU backend yet.
	static const char* const builtins[] = {
		"normalize", "length", "distance", "dot", "cross", "reflect", "refract",
		"faceforward", "mix", "clamp", "smoothstep", "step", "min", "max",
		"abs", "sign", "floor", "ceil", "fract", "round", "trunc", "mod",
		"pow", "sqrt", "inversesqrt", "exp", "exp2", "log", "log2",
		"sin", "cos", "tan", "asin", "acos", "atan", "atan2",
		"transpose", "inverse", "determinant",
	};
	for(const char* name : builtins)
		if(dict.Find(PyValue(name)) < 0)
			dict.Add(PyValue(name), PyValue::Function(name,
				[](const Vector<PyValue>&, void*) { return PyValue::None(); }, nullptr));
}

// ---------------------------------------------------------------------------
// Backend lifetime / compilation
// ---------------------------------------------------------------------------

bool CpuBackend::Initialize() {
	return true;
}

ShaderCompileResult CpuBackend::CompileShader(const ShaderDesc& desc) {
	PyValue module;
	ShaderCompileResult result = CompileShaderSource(desc, module);
	if(result.success) {
		// `module` is the retained entry-point function; its lambda owns the
		// globals dict the interpreter will resolve names against.
		RegisterCpuBackendBuiltins(module.GetLambdaRW().globals);
		modules.Add(module);
	}
	return result;
}

// ---------------------------------------------------------------------------
// Vertex stage
// ---------------------------------------------------------------------------

static vec3 ReadPosition(const VertexInput& in, int vertexIndex) {
	if(!in.IsBound()) return vec3(0, 0, 0);
	size_t base = in.offset + (size_t)vertexIndex * in.stride;
	if(base + 3 * sizeof(float) > in.size) return vec3(0, 0, 0);
	const float* f = (const float*)(in.data + base);
	return vec3(f[0], f[1], f[2]);
}

static void ImportVertexResult(const PyValue& result, VertexOutput& out) {
	int t = result.GetType();
	if(t == ::Upp::PY_TUPLE || t == ::Upp::PY_LIST) {
		const Vector<PyValue>& a = result.GetArray();
		if(a.GetCount() >= 1) ToVec4(a[0], out.clip);
		for(int i = 1; i < a.GetCount(); i++) {
			vec4 v(0, 0, 0, 0);
			ToVec4(a[i], v);
			out.varyings.Add(v);
		}
		return;
	}
	if(!ToVec4(result, out.clip))
		out.clip = vec4(0, 0, 0, 0);
}

void CpuBackend::ExecuteVertexShader(Shader* shader, Vector<VertexOutput>& out,
                                     const VertexInput& input, const Vector<int>& vertexIndices) {
	ShaderCPU* shaderCPU = static_cast<ShaderCPU*>(shader);
	if(!shaderCPU || !shaderCPU->GetPyLambda())
		return;

	PyLambda* lambda = shaderCPU->GetPyLambda();
	RegisterCpuBackendBuiltins(lambda->globals);
	PyVM vm;
	PyValue func(::Upp::PY_FUNCTION, lambda);

	int argCount = lambda->arg.GetCount();
	for(int slot : vertexIndices) {
		vec3 pos = ReadPosition(input, slot);

		Vector<PyValue> args;
		if(argCount >= 1)
			args.Add(PyValue::UserData(new PyVec3(pos[0], pos[1], pos[2])));
		for(int a = 1; a < argCount; a++)
			args.Add(PyValue::UserData(new PyVec4(0, 0, 0, 1)));

		VertexOutput vo;
		vo.inputVertex = slot;
		try {
			PyValue r = vm.Call(func, args);
			ImportVertexResult(r, vo);
		}
		catch(Upp::Exc& e) {
			LOG("Vertex shader error: " + e.ToString());
			vo.clip = vec4(0, 0, 0, 0);
		}
		out.Add(Upp::pick(vo));
	}
}

// ---------------------------------------------------------------------------
// Fragment stage
// ---------------------------------------------------------------------------

bool CpuBackend::ExecuteFragmentShader(Shader* shader, const Vector<vec4>& varyings, vec4& outColor) {
	ShaderCPU* shaderCPU = static_cast<ShaderCPU*>(shader);
	if(!shaderCPU || !shaderCPU->GetPyLambda())
		return false;

	PyLambda* lambda = shaderCPU->GetPyLambda();
	RegisterCpuBackendBuiltins(lambda->globals);
	PyVM vm;
	PyValue func(::Upp::PY_FUNCTION, lambda);

	int argCount = lambda->arg.GetCount();
	Vector<PyValue> args;
	for(int a = 0; a < argCount; a++) {
		vec4 v = a < varyings.GetCount() ? varyings[a] : vec4(0, 0, 0, 1);
		args.Add(PyValue::UserData(new PyVec4(v[0], v[1], v[2], v[3])));
	}
	try {
		PyValue r = vm.Call(func, args);
		return ToVec4(r, outColor);
	}
	catch(Upp::Exc& e) {
		LOG("Fragment shader error: " + e.ToString());
		return false;
	}
}

// ---------------------------------------------------------------------------
// Compute stage
// ---------------------------------------------------------------------------

void CpuBackend::ExecuteComputeShader(Shader* shader, const ShaderInterface& interface,
                                      void* workgroupData, int groupCountX, int groupCountY, int groupCountZ) {
	ShaderCPU* shaderCPU = static_cast<ShaderCPU*>(shader);
	if(!shaderCPU || !shaderCPU->GetPyLambda())
		return;

	PyLambda* lambda = shaderCPU->GetPyLambda();
	RegisterCpuBackendBuiltins(lambda->globals);
	PyVM vm;
	PyValue func(::Upp::PY_FUNCTION, lambda);

	int argCount = lambda->arg.GetCount();
	for(int z = 0; z < groupCountZ; z++) {
		for(int y = 0; y < groupCountY; y++) {
			for(int x = 0; x < groupCountX; x++) {
				Vector<PyValue> args;
				if(argCount == 3) {
					args.Add(PyValue(x));
					args.Add(PyValue(y));
					args.Add(PyValue(z));
				} else if(argCount >= 1) {
					int flatIndex = (z * groupCountY + y) * groupCountX + x;
					args.Add(PyValue(flatIndex));
				}
				try {
					vm.Call(func, args);
				}
				catch(Upp::Exc& e) {
					LOG("Compute shader error: " + e.ToString());
				}
			}
		}
	}
	(void)interface;
	(void)workgroupData;
}

// ---------------------------------------------------------------------------
// Rasterization
// ---------------------------------------------------------------------------

// Triangle setup + edge-function scanline fill. Vertex outputs are in clip
// space; they are perspective-divided and mapped through the viewport, then
// filled with perspective-correct barycentric varying interpolation.
void CpuBackend::Draw(Shader* vertexShader, Shader* fragmentShader, const DrawParams& p) {
	if(!vertexShader || !p.colorTarget)
		return;

	TextureCPU* color = dynamic_cast<TextureCPU*>(p.colorTarget);
	if(!color)
		return;

	const TextureDesc& cd = color->GetDesc();
	int cw = cd.width, ch = cd.height;
	byte* pixels = (byte*)color->GetData();
	if(!pixels || cw <= 0 || ch <= 0 || cd.format != TextureFormat::RGBA8)
		return;

	// Vertex indices for this draw (sequential for Draw, gathered for DrawIndexed).
	Vector<int> indices;
	if(p.indexData) {
		const Upp::uint32* idx = (const Upp::uint32*)(p.indexData + p.indexOffset);
		for(Upp::uint32 i = 0; i < p.indexCount; i++)
			indices.Add((int)idx[i] + (int)p.firstVertex);
	}
	else {
		for(Upp::uint32 i = 0; i < p.vertexCount; i++)
			indices.Add((int)(p.firstVertex + i));
	}
	if(indices.GetCount() < 3)
		return;

	Vector<VertexOutput> verts;
	ExecuteVertexShader(vertexShader, verts, p.vertexInput, indices);
	if(verts.GetCount() < 3)
		return;

	// Viewport defaults to the whole attachment when unset.
	float vx = p.viewport.x, vy = p.viewport.y;
	float vw = p.viewport.width  > 0 ? p.viewport.width  : (float)cw;
	float vh = p.viewport.height > 0 ? p.viewport.height : (float)ch;
	float mind = p.viewport.minDepth, maxd = p.viewport.maxDepth;

	int sx0 = p.scissor.width  > 0 ? p.scissor.x : 0;
	int sy0 = p.scissor.height > 0 ? p.scissor.y : 0;
	int sx1 = p.scissor.width  > 0 ? p.scissor.x + p.scissor.width  : cw;
	int sy1 = p.scissor.height > 0 ? p.scissor.y + p.scissor.height : ch;
	sx0 = Upp::max(0, sx0); sy0 = Upp::max(0, sy0);
	sx1 = Upp::min(cw, sx1); sy1 = Upp::min(ch, sy1);

	// Transient depth buffer (initialized to the far value for the compare op).
	bool useDepth = p.depthTest && p.depthTarget != nullptr;
	Vector<float> depth;
	if(useDepth) {
		depth.SetCount(cw * ch);
		float init = (p.depthCompare == CompareOp::Greater || p.depthCompare == CompareOp::GreaterOrEqual) ? 0.0f : 1.0f;
		for(float& d : depth) d = init;
	}

	auto depthPass = [&](float z, int idx) {
		if(!useDepth) return true;
		float old = depth[idx];
		bool ok;
		switch(p.depthCompare) {
			case CompareOp::Never:          ok = false; break;
			case CompareOp::Less:           ok = z <  old; break;
			case CompareOp::Equal:          ok = z == old; break;
			case CompareOp::LessOrEqual:    ok = z <= old; break;
			case CompareOp::Greater:        ok = z >  old; break;
			case CompareOp::NotEqual:       ok = z != old; break;
			case CompareOp::GreaterOrEqual: ok = z >= old; break;
			default:                        ok = true; break;
		}
		if(ok && p.depthWrite) depth[idx] = z;
		return ok;
	};

	auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
		return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
	};

	auto emitTriangle = [&](int ia, int ib, int ic) {
		const VertexOutput* V[3] = { &verts[ia], &verts[ib], &verts[ic] };
		for(int k = 0; k < 3; k++)
			if(V[k]->clip[3] <= 0)
				return; // no near-plane clipping yet

		float X[3], Y[3], Z[3], IW[3];
		for(int k = 0; k < 3; k++) {
			const vec4& c = V[k]->clip;
			float invW = 1.0f / c[3];
			X[k] = vx + ((c[0] * invW) * 0.5f + 0.5f) * vw;
			Y[k] = vy + (0.5f - (c[1] * invW) * 0.5f) * vh;  // flip Y so +Y is up
			Z[k] = mind + (c[2] * invW) * (maxd - mind);
			IW[k] = invW;
		}

		float area = edge(X[0], Y[0], X[1], Y[1], X[2], Y[2]);
		if(fabsf(area) < 1e-9f)
			return;
		if(area < 0) {
			Upp::Swap(X[1], X[2]);
			Upp::Swap(Y[1], Y[2]);
			Upp::Swap(Z[1], Z[2]);
			Upp::Swap(IW[1], IW[2]);
			Upp::Swap(V[1], V[2]);
			area = -area;
		}

		int minx = Upp::max(sx0, (int)floorf(Upp::min(X[0], Upp::min(X[1], X[2]))));
		int maxx = Upp::min(sx1 - 1, (int)ceilf(Upp::max(X[0], Upp::max(X[1], X[2]))));
		int miny = Upp::max(sy0, (int)floorf(Upp::min(Y[0], Upp::min(Y[1], Y[2]))));
		int maxy = Upp::min(sy1 - 1, (int)ceilf(Upp::max(Y[0], Upp::max(Y[1], Y[2]))));

		int nVar = Upp::max(V[0]->varyings.GetCount(),
		                    Upp::max(V[1]->varyings.GetCount(), V[2]->varyings.GetCount()));
		for(int py = miny; py <= maxy; py++) {
			for(int px = minx; px <= maxx; px++) {
				float fx = px + 0.5f, fy = py + 0.5f;
				float w0 = edge(X[1], Y[1], X[2], Y[2], fx, fy);
				float w1 = edge(X[2], Y[2], X[0], Y[0], fx, fy);
				float w2 = edge(X[0], Y[0], X[1], Y[1], fx, fy);
				if(w0 < 0 || w1 < 0 || w2 < 0)
					continue;

				float l0 = w0 / area, l1 = w1 / area, l2 = w2 / area;
				float z = l0 * Z[0] + l1 * Z[1] + l2 * Z[2];
				int pidx = py * cw + px;
				if(!depthPass(z, pidx))
					continue;

				// Perspective-correct varying interpolation.
				float iw = l0 * IW[0] + l1 * IW[1] + l2 * IW[2];
				float k0 = (iw != 0) ? l0 * IW[0] / iw : l0;
				float k1 = (iw != 0) ? l1 * IW[1] / iw : l1;
				float k2 = (iw != 0) ? l2 * IW[2] / iw : l2;

				Vector<vec4> fragVaryings;
				fragVaryings.SetCount(nVar);
				for(int v = 0; v < nVar; v++) {
					vec4 va = v < V[0]->varyings.GetCount() ? V[0]->varyings[v] : vec4(0, 0, 0, 0);
					vec4 vb = v < V[1]->varyings.GetCount() ? V[1]->varyings[v] : vec4(0, 0, 0, 0);
					vec4 vc = v < V[2]->varyings.GetCount() ? V[2]->varyings[v] : vec4(0, 0, 0, 0);
					fragVaryings[v] = vec4(k0 * va[0] + k1 * vb[0] + k2 * vc[0],
					                       k0 * va[1] + k1 * vb[1] + k2 * vc[1],
					                       k0 * va[2] + k1 * vb[2] + k2 * vc[2],
					                       k0 * va[3] + k1 * vb[3] + k2 * vc[3]);
				}

				vec4 outColor(0, 0, 0, 1);
				if(fragmentShader && !ExecuteFragmentShader(fragmentShader, fragVaryings, outColor))
					continue;

				byte* dst = pixels + (size_t)pidx * 4;
				dst[0] = (byte)Upp::clamp((int)lroundf(outColor[0] * 255.0f), 0, 255);
				dst[1] = (byte)Upp::clamp((int)lroundf(outColor[1] * 255.0f), 0, 255);
				dst[2] = (byte)Upp::clamp((int)lroundf(outColor[2] * 255.0f), 0, 255);
				dst[3] = (byte)Upp::clamp((int)lroundf(outColor[3] * 255.0f), 0, 255);
			}
		}
	};

	switch(p.topology) {
		case PrimitiveTopology::Triangles:
			for(int i = 0; i + 2 < verts.GetCount(); i += 3)
				emitTriangle(i, i + 1, i + 2);
			break;
		case PrimitiveTopology::TriangleStrip:
			for(int i = 0; i + 2 < verts.GetCount(); i++)
				emitTriangle(i, i + 1, i + 2);
			break;
		case PrimitiveTopology::TriangleFan:
			for(int i = 1; i + 1 < verts.GetCount(); i++)
				emitTriangle(0, i, i + 1);
			break;
		default:
			break; // Points / Lines not rasterized yet
	}
}

} // namespace SoftGraphics
