#pragma once

#include <Core/Core.h>
#include <ByteVM/Python/Python.h>
#include <Geometry/Geometry.h>

namespace SoftGraphics {

using Upp::One;
using Upp::Array;
using Upp::String;
using Upp::PyLambda;

// Math types from Geometry
using Upp::vec2;
using Upp::vec3;
using Upp::vec4;
using Upp::mat4;

using Vec2 = vec2;
using Vec3 = vec3;
using Vec4 = vec4;
using Mat4 = mat4;

// Forward declarations
class Device;
class Instance;
class Surface;
class Buffer;
class Texture;
class Shader;
class Pipeline;
class Framebuffer;
class CommandBuffer;

// Enumerations
enum class Backend {
	CPU,
	Vulkan,
	Metal,
	DX12,
	OpenGL,
	Cinematic,
};

enum class BufferUsage {
	Vertex,
	Index,
	Uniform,
	Storage,
	Indirect,
};

enum class TextureType {
	Tex1D,
	Tex2D,
	Tex3D,
	Cube,
	Tex2DArray,
};

enum class TextureFormat {
	R8,
	RG8,
	RGB8,
	RGBA8,
	R16F,
	RG16F,
	RGB16F,
	RGBA16F,
	R32F,
	RG32F,
	RGB32F,
	RGBA32F,
	D16,
	D24,
	D32F,
	D24S8,
};

enum class ShaderStage {
	Vertex,
	Fragment,
	Compute,
};

enum class PrimitiveTopology {
	Points,
	Lines,
	LineStrip,
	Triangles,
	TriangleStrip,
	TriangleFan,
};

enum class CompareOp {
	Never,
	Less,
	Equal,
	LessOrEqual,
	Greater,
	NotEqual,
	GreaterOrEqual,
	Always,
};

enum class BlendFactor {
	Zero,
	One,
	SrcColor,
	OneMinusSrcColor,
	DstColor,
	OneMinusDstColor,
	SrcAlpha,
	OneMinusSrcAlpha,
	DstAlpha,
	OneMinusDstAlpha,
};

enum class BlendOp {
	Add,
	Subtract,
	ReverseSubtract,
	Min,
	Max,
};

enum class ResourceState {
	Undefined,
	General,
	VertexBuffer,
	IndexBuffer,
	UniformBuffer,
	StorageBuffer,
	RenderTarget,
	DepthStencilRead,
	DepthStencilWrite,
	ShaderResource,
	UnorderedAccess,
	Present,
};

// Structures
struct BufferDesc {
	BufferUsage usage = BufferUsage::Vertex;
	size_t size = 0;
	void* initialData = nullptr;
};

struct TextureDesc {
	TextureType type = TextureType::Tex2D;
	TextureFormat format = TextureFormat::RGBA8;
	int width = 1;
	int height = 1;
	int depth = 1;
	int arrayLayers = 1;
	int mipLevels = 1;
};

struct ShaderDesc {
	ShaderStage stage;
	String source;  // Typed Python source code
	String entryPoint = "main";
};

struct VertexAttribute {
	String name;
	TextureFormat format;  // Reused for attribute format
	uint32_t offset = 0;
	uint32_t binding = 0;
};

struct VertexInputDesc {
	Array<VertexAttribute> attributes;
	uint32_t stride = 0;
};

struct ColorBlendAttachment {
	bool blendEnable = false;
	BlendFactor srcColorBlendFactor = BlendFactor::One;
	BlendFactor dstColorBlendFactor = BlendFactor::Zero;
	BlendOp colorBlendOp = BlendOp::Add;
	BlendFactor srcAlphaBlendFactor = BlendFactor::One;
	BlendFactor dstAlphaBlendFactor = BlendFactor::Zero;
	BlendOp alphaBlendOp = BlendOp::Add;
	uint32_t colorWriteMask = 0xF;
};

struct DepthStencilDesc {
	bool depthTestEnable = true;
	bool depthWriteEnable = true;
	CompareOp depthCompareOp = CompareOp::Less;
	bool stencilTestEnable = false;
};

struct GraphicsPipelineDesc {
	Array<Shader*> shaders;
	VertexInputDesc vertexInput;
	PrimitiveTopology topology = PrimitiveTopology::Triangles;
	ColorBlendAttachment colorBlend;
	DepthStencilDesc depthStencil;
	bool rasterizerDiscard = false;
	
	GraphicsPipelineDesc() = default;
	GraphicsPipelineDesc(const GraphicsPipelineDesc&) = delete;
	GraphicsPipelineDesc& operator=(const GraphicsPipelineDesc&) = delete;
	GraphicsPipelineDesc(GraphicsPipelineDesc&&) = default;
	GraphicsPipelineDesc& operator=(GraphicsPipelineDesc&&) = default;
};

struct ComputePipelineDesc {
	Shader* shader;
};

struct AttachmentDesc {
	TextureFormat format;
	ResourceState initialState = ResourceState::Undefined;
	ResourceState finalState = ResourceState::Undefined;
};

struct FramebufferDesc {
	Array<Texture*> colorAttachments;
	Texture* depthStencilAttachment;
	
	FramebufferDesc() = default;
	FramebufferDesc(const FramebufferDesc&) = delete;
	FramebufferDesc& operator=(const FramebufferDesc&) = delete;
	FramebufferDesc(FramebufferDesc&&) = default;
	FramebufferDesc& operator=(FramebufferDesc&&) = default;
};

struct Viewport {
	float x = 0, y = 0;
	float width = 0, height = 0;
	float minDepth = 0, maxDepth = 1;
};

struct Scissor {
	int x = 0, y = 0;
	int width = 0, height = 0;
};

// Main API functions
Instance* sgCreateInstance();
void sgDestroyInstance(Instance* instance);

Array<Device*> sgEnumerateDevices(Instance* instance, Backend preferredBackend = Backend::CPU);

Device* sgCreateDevice(Instance* instance, Device* physicalDevice, Backend backend);
void sgDestroyDevice(Device* device);

Surface* sgCreateSurface(Device* device, void* windowHandle);
void sgDestroySurface(Surface* surface);

Buffer* sgCreateBuffer(Device* device, const BufferDesc& desc);
void sgDestroyBuffer(Buffer* buffer);

Texture* sgCreateTexture(Device* device, const TextureDesc& desc);
void sgDestroyTexture(Texture* texture);

Shader* sgCreateShader(Device* device, const ShaderDesc& desc);
void sgDestroyShader(Shader* shader);

Pipeline* sgCreateGraphicsPipeline(Device* device, GraphicsPipelineDesc&& desc);
Pipeline* sgCreateComputePipeline(Device* device, ComputePipelineDesc&& desc);
void sgDestroyPipeline(Pipeline* pipeline);

Framebuffer* sgCreateFramebuffer(Device* device, FramebufferDesc&& desc);
void sgDestroyFramebuffer(Framebuffer* framebuffer);

CommandBuffer* sgCreateCommandBuffer(Device* device);
void sgDestroyCommandBuffer(CommandBuffer* cmd);

void sgBeginCommandBuffer(CommandBuffer* cmd);
void sgEndCommandBuffer(CommandBuffer* cmd);

void sgCmdBeginRenderPass(CommandBuffer* cmd, Framebuffer* framebuffer, const Viewport& viewport, const Scissor& scissor);
void sgCmdEndRenderPass(CommandBuffer* cmd);
void sgCmdBindPipeline(CommandBuffer* cmd, Pipeline* pipeline);
void sgCmdBindVertexBuffers(CommandBuffer* cmd, uint32_t firstBinding, Array<Buffer*>&& buffers, Array<uint64_t>&& offsets);
void sgCmdBindIndexBuffer(CommandBuffer* cmd, Buffer* buffer, uint64_t offset);
void sgCmdDraw(CommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void sgCmdDrawIndexed(CommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);
void sgCmdDispatch(CommandBuffer* cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

void sgQueueSubmit(Device* device, const Array<CommandBuffer*>& commandBuffers);
void sgDeviceWaitIdle(Device* device);

void sgSetObjectName(Device* device, void* object, const char* name);

// Typed Python shader compilation
struct ShaderCompileResult {
	bool success = false;
	String error;
	PyLambda* pyLambda = nullptr;  // Compiled typed PyIR
};

ShaderCompileResult sgCompileShader(Device* device, const ShaderDesc& desc);

} // namespace SoftGraphics