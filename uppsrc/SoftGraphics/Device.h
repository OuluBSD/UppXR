#pragma once

#include "SoftGraphics.h"
#include "CpuBackend.h"

namespace SoftGraphics {

class Instance {
public:
	Instance() = default;
	virtual ~Instance() = default;

	// Enumerate available physical devices
	virtual Array<Device*> EnumerateDevices(Backend preferredBackend = Backend::CPU) = 0;

protected:
	Instance(const Instance&) = delete;
	Instance& operator=(const Instance&) = delete;
};

class Device {
public:
	Device() = default;
	virtual ~Device() = default;

	// Device properties
	virtual String GetName() const = 0;
	virtual Backend GetBackend() const = 0;

	// Resource creation
	virtual Buffer* CreateBuffer(const BufferDesc& desc) = 0;
	virtual Texture* CreateTexture(const TextureDesc& desc) = 0;
	virtual Shader* CreateShader(const ShaderDesc& desc) = 0;
	virtual Pipeline* CreateGraphicsPipeline(GraphicsPipelineDesc&& desc) = 0;
	virtual Pipeline* CreateComputePipeline(ComputePipelineDesc&& desc) = 0;
	virtual Framebuffer* CreateFramebuffer(FramebufferDesc&& desc) = 0;
	virtual CommandBuffer* CreateCommandBuffer() = 0;

	// Command submission
	virtual void QueueSubmit(const Array<CommandBuffer*>& commandBuffers) = 0;
	virtual void WaitIdle() = 0;

	// Shader compilation
	virtual ShaderCompileResult CompileShader(const ShaderDesc& desc) = 0;

	// Debug
	virtual void SetObjectName(void* object, const char* name) = 0;

protected:
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
};

// CPU backend device (validation backend using PyVM + SoftRend)
class DeviceCPU : public Device {
public:
	DeviceCPU() = default;
	~DeviceCPU() override = default;

	String GetName() const override { return "SoftGraphics CPU (PyVM + SoftRend)"; }
	Backend GetBackend() const override { return Backend::CPU; }

	Buffer* CreateBuffer(const BufferDesc& desc) override;
	Texture* CreateTexture(const TextureDesc& desc) override;
	Shader* CreateShader(const ShaderDesc& desc) override;
	Pipeline* CreateGraphicsPipeline(GraphicsPipelineDesc&& desc) override;
	Pipeline* CreateComputePipeline(ComputePipelineDesc&& desc) override;
	Framebuffer* CreateFramebuffer(FramebufferDesc&& desc) override;
	CommandBuffer* CreateCommandBuffer() override;

	void QueueSubmit(const Array<CommandBuffer*>& commandBuffers) override;
	void WaitIdle() override;

	ShaderCompileResult CompileShader(const ShaderDesc& desc) override;

	void SetObjectName(void* object, const char* name) override;

private:
	Array<CommandBuffer*> pendingCommands;
	Upp::Vector<Upp::PyValue> shaderModules;
	CpuBackend cpuBackend;
};

} // namespace SoftGraphics