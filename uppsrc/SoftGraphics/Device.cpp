#include "Device.h"
#include "Instance.h"
#include "Buffer.h"
#include "Texture.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Framebuffer.h"
#include "CommandBuffer.h"
#include "ShaderIR.h"
#include "CpuBackend.h"

namespace SoftGraphics {

// InstanceImpl
Array<Device*> InstanceImpl::EnumerateDevices(Backend preferredBackend) {
	Array<Device*> devices;
	
	// Always add CPU device
	devices.Add(new DeviceCPU());
	
	// TODO: Add Vulkan, Metal, DX12, OpenGL devices when backends are implemented
	
	return devices;
}

// DeviceCPU - minimal stubs, actual CPU backend implementation in CpuBackend package
Buffer* DeviceCPU::CreateBuffer(const BufferDesc& desc) {
	return new BufferCPU(desc);
}

Texture* DeviceCPU::CreateTexture(const TextureDesc& desc) {
	return new TextureCPU(desc);
}

Shader* DeviceCPU::CreateShader(const ShaderDesc& desc) {
	return new ShaderCPU(desc, CompileShader(desc));
}

Pipeline* DeviceCPU::CreateGraphicsPipeline(GraphicsPipelineDesc&& desc) {
	return new PipelineCPU(std::move(desc));
}

Pipeline* DeviceCPU::CreateComputePipeline(ComputePipelineDesc&& desc) {
	return new PipelineCPU(std::move(desc));
}

Framebuffer* DeviceCPU::CreateFramebuffer(FramebufferDesc&& desc) {
	return new FramebufferCPU(std::move(desc));
}

CommandBuffer* DeviceCPU::CreateCommandBuffer() {
	return new CommandBufferCPU();
}

void DeviceCPU::QueueSubmit(const Array<CommandBuffer*>& commandBuffers) {
	for (auto* cmd : commandBuffers) {
		pendingCommands.Add(cmd);
	}
}

// Clear a render-target texture to zero. There is no load-op in the API yet, so
// a render pass begins from a defined (black / far-depth) state.
static void ClearTarget(Texture* tex, float depthValue, bool isDepth) {
	TextureCPU* t = dynamic_cast<TextureCPU*>(tex);
	if (!t) return;
	byte* data = (byte*)t->GetData();
	const TextureDesc& d = t->GetDesc();
	size_t count = (size_t)d.width * d.height * d.depth * d.arrayLayers;
	if (!data || !count) return;
	if (isDepth && d.format == TextureFormat::D32F) {
		float* f = (float*)data;
		for (size_t i = 0; i < count; i++) f[i] = depthValue;
	}
	else {
		memset(data, 0, count * 4);  // RGBA8
	}
}

void DeviceCPU::WaitIdle() {
	if (!cpuBackend.Initialize()) return;

	for (auto* cmdBuf : pendingCommands) {
		CommandBufferCPU* cmd = static_cast<CommandBufferCPU*>(cmdBuf);
		if (!cmd) continue;

		PipelineCPU* currentPipeline = nullptr;
		FramebufferCPU* currentFramebuffer = nullptr;
		Viewport currentViewport;
		Scissor currentScissor;
		Array<Buffer*> boundVertexBuffers;
		Array<uint64_t> boundVertexOffsets;
		Buffer* boundIndexBuffer = nullptr;
		uint64_t boundIndexOffset = 0;

		for (auto& recordedCmd : cmd->commands) {
			switch (recordedCmd.type) {
				case CommandBufferCPU::RecordedCommand::Type::BeginRenderPass: {
					currentFramebuffer = static_cast<FramebufferCPU*>(recordedCmd.framebuffer);
					currentViewport = recordedCmd.viewport;
					currentScissor = recordedCmd.scissor;
					if (currentFramebuffer) {
						const FramebufferDesc& fb = currentFramebuffer->GetDesc();
						for (Texture* c : fb.colorAttachments)
							ClearTarget(c, 0.0f, false);
						ClearTarget(fb.depthStencilAttachment, 1.0f, true);
					}
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::EndRenderPass: {
					currentFramebuffer = nullptr;
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::BindPipeline: {
					currentPipeline = static_cast<PipelineCPU*>(recordedCmd.pipeline);
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::BindVertexBuffers: {
					boundVertexBuffers = pick(recordedCmd.vertexBuffers);
					boundVertexOffsets = pick(recordedCmd.vertexOffsets);
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::BindIndexBuffer: {
					boundIndexBuffer = recordedCmd.indexBuffer;
					boundIndexOffset = recordedCmd.indexOffset;
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::Draw: {
					if (!currentPipeline || !currentPipeline->GetGraphicsDesc())
						break;
					const GraphicsPipelineDesc* pipelineDesc = currentPipeline->GetGraphicsDesc();
					if (pipelineDesc->shaders.IsEmpty() || !currentFramebuffer)
						break;
					const FramebufferDesc& fb = currentFramebuffer->GetDesc();
					if (fb.colorAttachments.IsEmpty())
						break;

					DrawParams dp;
					dp.vertexCount = recordedCmd.vertexCount;
					dp.instanceCount = recordedCmd.instanceCount;
					dp.firstVertex = recordedCmd.firstVertex;
					dp.topology = pipelineDesc->topology;
					dp.viewport = currentViewport;
					dp.scissor = currentScissor;
					dp.colorTarget = fb.colorAttachments[0];
					dp.depthTarget = fb.depthStencilAttachment;
					dp.depthTest = pipelineDesc->depthStencil.depthTestEnable;
					dp.depthWrite = pipelineDesc->depthStencil.depthWriteEnable;
					dp.depthCompare = pipelineDesc->depthStencil.depthCompareOp;

					if (!boundVertexBuffers.IsEmpty()) {
						BufferCPU* vb = dynamic_cast<BufferCPU*>(boundVertexBuffers[0]);
						if (vb) {
							dp.vertexInput.data = vb->GetData();
							dp.vertexInput.size = vb->GetSize();
							dp.vertexInput.stride = pipelineDesc->vertexInput.stride
								? pipelineDesc->vertexInput.stride : 3 * sizeof(float);
							dp.vertexInput.offset = boundVertexOffsets.IsEmpty() ? 0 : (size_t)boundVertexOffsets[0];
						}
					}

					Shader* vs = pipelineDesc->shaders[0];
					Shader* fs = pipelineDesc->shaders.GetCount() > 1 ? pipelineDesc->shaders[1] : nullptr;
					cpuBackend.Draw(vs, fs, dp);
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::DrawIndexed: {
					if (!currentPipeline || !currentPipeline->GetGraphicsDesc())
						break;
					const GraphicsPipelineDesc* pipelineDesc = currentPipeline->GetGraphicsDesc();
					if (pipelineDesc->shaders.IsEmpty() || !currentFramebuffer)
						break;
					const FramebufferDesc& fb = currentFramebuffer->GetDesc();
					if (fb.colorAttachments.IsEmpty())
						break;

					DrawParams dp;
					dp.vertexCount = recordedCmd.indexCount;
					dp.indexCount = recordedCmd.indexCount;
					dp.instanceCount = recordedCmd.instanceCount;
					dp.firstVertex = (uint32_t)Upp::max(0, (int)recordedCmd.vertexOffset);
					dp.topology = pipelineDesc->topology;
					dp.viewport = currentViewport;
					dp.scissor = currentScissor;
					dp.colorTarget = fb.colorAttachments[0];
					dp.depthTarget = fb.depthStencilAttachment;
					dp.depthTest = pipelineDesc->depthStencil.depthTestEnable;
					dp.depthWrite = pipelineDesc->depthStencil.depthWriteEnable;
					dp.depthCompare = pipelineDesc->depthStencil.depthCompareOp;

					if (!boundVertexBuffers.IsEmpty()) {
						BufferCPU* vb = dynamic_cast<BufferCPU*>(boundVertexBuffers[0]);
						if (vb) {
							dp.vertexInput.data = vb->GetData();
							dp.vertexInput.size = vb->GetSize();
							dp.vertexInput.stride = pipelineDesc->vertexInput.stride
								? pipelineDesc->vertexInput.stride : 3 * sizeof(float);
							dp.vertexInput.offset = boundVertexOffsets.IsEmpty() ? 0 : (size_t)boundVertexOffsets[0];
						}
					}
					if (boundIndexBuffer) {
						BufferCPU* ib = dynamic_cast<BufferCPU*>(boundIndexBuffer);
						if (ib) {
							dp.indexData = ib->GetData();
							dp.indexOffset = (size_t)boundIndexOffset;
						}
					}

					Shader* vs = pipelineDesc->shaders[0];
					Shader* fs = pipelineDesc->shaders.GetCount() > 1 ? pipelineDesc->shaders[1] : nullptr;
					cpuBackend.Draw(vs, fs, dp);
					break;
				}
				case CommandBufferCPU::RecordedCommand::Type::Dispatch: {
					if (currentPipeline && currentPipeline->GetComputeDesc()) {
						const ComputePipelineDesc* pipelineDesc = currentPipeline->GetComputeDesc();
						if (pipelineDesc->shader) {
							ShaderInterface interface;
							cpuBackend.ExecuteComputeShader(pipelineDesc->shader, interface,
								nullptr, recordedCmd.groupCountX, recordedCmd.groupCountY, recordedCmd.groupCountZ);
						}
					}
					break;
				}
			}
		}
	}
	pendingCommands.Clear();
}

ShaderCompileResult DeviceCPU::CompileShader(const ShaderDesc& desc) {
	Upp::PyValue module;
	ShaderCompileResult result = CompileShaderSource(desc, module);
	if(result.success) {
		RegisterCpuBackendBuiltins(module.GetLambdaRW().globals);
		shaderModules.Add(module);
	}
	return result;
}

void DeviceCPU::SetObjectName(void* object, const char* name) {
	// Debug naming - no-op for CPU backend
}

} // namespace SoftGraphics
