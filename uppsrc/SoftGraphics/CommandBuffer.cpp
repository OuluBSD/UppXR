#include "CommandBuffer.h"
#include "Device.h"

namespace SoftGraphics {

void CommandBufferCPU::Begin() {
	recording = true;
	commands.Clear();
}

void CommandBufferCPU::End() {
	recording = false;
}

void CommandBufferCPU::CmdBeginRenderPass(Framebuffer* framebuffer, const Viewport& viewport, const Scissor& scissor) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::BeginRenderPass;
	cmd.framebuffer = framebuffer;
	cmd.viewport = viewport;
	cmd.scissor = scissor;
	commands.Add(std::move(cmd));
	currentFramebuffer = framebuffer;
}

void CommandBufferCPU::CmdEndRenderPass() {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::EndRenderPass;
	commands.Add(std::move(cmd));
	currentFramebuffer = nullptr;
}

void CommandBufferCPU::CmdBindPipeline(Pipeline* pipeline) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::BindPipeline;
	cmd.pipeline = pipeline;
	commands.Add(std::move(cmd));
	currentPipeline = pipeline;
}

void CommandBufferCPU::CmdBindVertexBuffers(uint32_t firstBinding, Array<Buffer*>&& buffers, Array<uint64_t>&& offsets) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::BindVertexBuffers;
	cmd.vertexBuffers = std::move(buffers);
	cmd.vertexOffsets = std::move(offsets);
	commands.Add(std::move(cmd));
}

void CommandBufferCPU::CmdBindIndexBuffer(Buffer* buffer, uint64_t offset) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::BindIndexBuffer;
	cmd.indexBuffer = buffer;
	cmd.indexOffset = offset;
	commands.Add(std::move(cmd));
}

void CommandBufferCPU::CmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::Draw;
	cmd.vertexCount = vertexCount;
	cmd.instanceCount = instanceCount;
	cmd.firstVertex = firstVertex;
	cmd.firstInstance = firstInstance;
	commands.Add(std::move(cmd));
}

void CommandBufferCPU::CmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::DrawIndexed;
	cmd.indexCount = indexCount;
	cmd.instanceCount = instanceCount;
	cmd.firstIndex = firstIndex;
	cmd.vertexOffset = vertexOffset;
	cmd.firstInstance = firstInstance;
	commands.Add(std::move(cmd));
}

void CommandBufferCPU::CmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
	RecordedCommand cmd;
	cmd.type = RecordedCommand::Type::Dispatch;
	cmd.groupCountX = groupCountX;
	cmd.groupCountY = groupCountY;
	cmd.groupCountZ = groupCountZ;
	commands.Add(std::move(cmd));
}

CommandBuffer* sgCreateCommandBuffer(Device* device) {
	return device->CreateCommandBuffer();
}

void sgDestroyCommandBuffer(CommandBuffer* cmd) {
	delete cmd;
}

void sgBeginCommandBuffer(CommandBuffer* cmd) {
	cmd->Begin();
}

void sgEndCommandBuffer(CommandBuffer* cmd) {
	cmd->End();
}

void sgCmdBeginRenderPass(CommandBuffer* cmd, Framebuffer* framebuffer, const Viewport& viewport, const Scissor& scissor) {
	cmd->CmdBeginRenderPass(framebuffer, viewport, scissor);
}

void sgCmdEndRenderPass(CommandBuffer* cmd) {
	cmd->CmdEndRenderPass();
}

void sgCmdBindPipeline(CommandBuffer* cmd, Pipeline* pipeline) {
	cmd->CmdBindPipeline(pipeline);
}

void sgCmdBindVertexBuffers(CommandBuffer* cmd, uint32_t firstBinding, Array<Buffer*>&& buffers, Array<uint64_t>&& offsets) {
	cmd->CmdBindVertexBuffers(firstBinding, std::move(buffers), std::move(offsets));
}

void sgCmdBindIndexBuffer(CommandBuffer* cmd, Buffer* buffer, uint64_t offset) {
	cmd->CmdBindIndexBuffer(buffer, offset);
}

void sgCmdDraw(CommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
	cmd->CmdDraw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void sgCmdDrawIndexed(CommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
	cmd->CmdDrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void sgCmdDispatch(CommandBuffer* cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
	cmd->CmdDispatch(groupCountX, groupCountY, groupCountZ);
}

void sgQueueSubmit(Device* device, const Array<CommandBuffer*>& commandBuffers) {
	device->QueueSubmit(commandBuffers);
}

void sgDeviceWaitIdle(Device* device) {
	device->WaitIdle();
}

void sgSetObjectName(Device* device, void* object, const char* name) {
	device->SetObjectName(object, name);
}

} // namespace SoftGraphics