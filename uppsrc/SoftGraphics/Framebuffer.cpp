#include "Framebuffer.h"
#include "Texture.h"
#include "Device.h"

namespace SoftGraphics {

FramebufferCPU::FramebufferCPU(FramebufferDesc&& desc) : Framebuffer(std::move(desc)) {
	for (auto& tex : desc.colorAttachments) {
		colorAttachments.Add(tex);
	}
	if (desc.depthStencilAttachment) {
		depthStencilAttachment = desc.depthStencilAttachment;
	}
}

Framebuffer* sgCreateFramebuffer(Device* device, FramebufferDesc&& desc) {
	return device->CreateFramebuffer(std::move(desc));
}

void sgDestroyFramebuffer(Framebuffer* framebuffer) {
	delete framebuffer;
}

} // namespace SoftGraphics