#pragma once

#include "SoftGraphics.h"

namespace SoftGraphics {

class Framebuffer {
public:
	Framebuffer(FramebufferDesc&& desc) : desc(std::move(desc)) {}
	virtual ~Framebuffer() = default;

	const FramebufferDesc& GetDesc() const { return desc; }

protected:
	FramebufferDesc desc;
	Framebuffer(const Framebuffer&) = delete;
	Framebuffer& operator=(const Framebuffer&) = delete;
	Framebuffer(Framebuffer&&) = default;
	Framebuffer& operator=(Framebuffer&&) = default;
};

class FramebufferCPU : public Framebuffer {
public:
	FramebufferCPU(FramebufferDesc&& desc);
	~FramebufferCPU() override = default;

private:
	Array<Texture*> colorAttachments;
	Texture* depthStencilAttachment;
};

} // namespace SoftGraphics