#include "Pipeline.h"
#include "Device.h"

namespace SoftGraphics {

PipelineCPU::PipelineCPU(GraphicsPipelineDesc&& desc) : Pipeline(Type::Graphics) {
	graphicsDesc = std::move(desc);
	isGraphics = true;
}

PipelineCPU::PipelineCPU(ComputePipelineDesc&& desc) : Pipeline(Type::Compute) {
	computeDesc = std::move(desc);
	isGraphics = false;
}

const GraphicsPipelineDesc* PipelineCPU::GetGraphicsDesc() const {
	return isGraphics ? &graphicsDesc : nullptr;
}

const ComputePipelineDesc* PipelineCPU::GetComputeDesc() const {
	return !isGraphics ? &computeDesc : nullptr;
}

Pipeline* sgCreateGraphicsPipeline(Device* device, GraphicsPipelineDesc&& desc) {
	return device->CreateGraphicsPipeline(std::move(desc));
}

Pipeline* sgCreateComputePipeline(Device* device, ComputePipelineDesc&& desc) {
	return device->CreateComputePipeline(std::move(desc));
}

void sgDestroyPipeline(Pipeline* pipeline) {
	delete pipeline;
}

} // namespace SoftGraphics