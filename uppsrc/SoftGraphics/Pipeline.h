#pragma once

#include "SoftGraphics.h"

namespace SoftGraphics {

class Pipeline {
public:
	enum class Type {
		Graphics,
		Compute,
	};

	Pipeline(Type type) : type(type) {}
	virtual ~Pipeline() = default;

	Type GetType() const { return type; }

protected:
	Type type;
	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;
	Pipeline(Pipeline&&) = default;
	Pipeline& operator=(Pipeline&&) = default;
};

class PipelineCPU : public Pipeline {
public:
	PipelineCPU(GraphicsPipelineDesc&& desc);
	PipelineCPU(ComputePipelineDesc&& desc);
	~PipelineCPU() override = default;

	const GraphicsPipelineDesc* GetGraphicsDesc() const;
	const ComputePipelineDesc* GetComputeDesc() const;

private:
	GraphicsPipelineDesc graphicsDesc;
	ComputePipelineDesc computeDesc;
	bool isGraphics = false;
};

} // namespace SoftGraphics