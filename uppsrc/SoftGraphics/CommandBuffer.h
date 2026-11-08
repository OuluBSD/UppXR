#pragma once

#include "SoftGraphics.h"

namespace SoftGraphics {

class CommandBuffer {
public:
	CommandBuffer() = default;
	virtual ~CommandBuffer() = default;

	virtual void Begin() = 0;
	virtual void End() = 0;

	// Render pass
	virtual void CmdBeginRenderPass(Framebuffer* framebuffer, const Viewport& viewport, const Scissor& scissor) = 0;
	virtual void CmdEndRenderPass() = 0;

	// Pipeline
	virtual void CmdBindPipeline(Pipeline* pipeline) = 0;

	// Vertex/Index buffers
	virtual void CmdBindVertexBuffers(uint32_t firstBinding, Array<Buffer*>&& buffers, Array<uint64_t>&& offsets) = 0;
	virtual void CmdBindIndexBuffer(Buffer* buffer, uint64_t offset) = 0;

	// Draw commands
	virtual void CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
	virtual void CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;

	// Compute
	virtual void CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

protected:
	CommandBuffer(const CommandBuffer&) = delete;
	CommandBuffer& operator=(const CommandBuffer&) = delete;
	CommandBuffer(CommandBuffer&&) = default;
	CommandBuffer& operator=(CommandBuffer&&) = default;
};

class CommandBufferCPU : public CommandBuffer {
public:
	CommandBufferCPU() = default;
	~CommandBufferCPU() override = default;
	CommandBufferCPU(CommandBufferCPU&&) = default;
	CommandBufferCPU& operator=(CommandBufferCPU&&) = default;

	void Begin() override;
	void End() override;

	void CmdBeginRenderPass(Framebuffer* framebuffer, const Viewport& viewport, const Scissor& scissor) override;
	void CmdEndRenderPass() override;
	void CmdBindPipeline(Pipeline* pipeline) override;
	void CmdBindVertexBuffers(uint32_t firstBinding, Array<Buffer*>&& buffers, Array<uint64_t>&& offsets) override;
	void CmdBindIndexBuffer(Buffer* buffer, uint64_t offset) override;
	void CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
	void CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;
	void CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

	struct RecordedCommand {
		enum class Type {
			BeginRenderPass,
			EndRenderPass,
			BindPipeline,
			BindVertexBuffers,
			BindIndexBuffer,
			Draw,
			DrawIndexed,
			Dispatch,
		} type;

		// Union-like storage for different command types
		Framebuffer* framebuffer = nullptr;
		Viewport viewport;
		Scissor scissor;
		Pipeline* pipeline = nullptr;
		Array<Buffer*> vertexBuffers;
		Array<uint64_t> vertexOffsets;
		Buffer* indexBuffer = nullptr;
		uint64_t indexOffset = 0;
		uint32_t vertexCount = 0, instanceCount = 0, firstVertex = 0, firstInstance = 0;
		uint32_t indexCount = 0, firstIndex = 0, vertexOffset = 0;
		uint32_t groupCountX = 0, groupCountY = 0, groupCountZ = 0;

		RecordedCommand() = default;
		RecordedCommand(const RecordedCommand&) = delete;
		RecordedCommand& operator=(const RecordedCommand&) = delete;
		RecordedCommand(RecordedCommand&&) = default;
		RecordedCommand& operator=(RecordedCommand&&) = default;
	};

	Array<RecordedCommand> commands;
	bool recording = false;
	Framebuffer* currentFramebuffer = nullptr;
	Pipeline* currentPipeline = nullptr;
};

} // namespace SoftGraphics