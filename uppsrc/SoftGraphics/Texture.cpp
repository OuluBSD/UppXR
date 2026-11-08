#include "Texture.h"
#include "Device.h"
#include <algorithm>

namespace SoftGraphics {

TextureCPU::TextureCPU(const TextureDesc& desc) : Texture(desc) {
	// Calculate total data size for all mip levels and array layers
	size_t pixelSize = 0;
	switch (desc.format) {
		case TextureFormat::R8: pixelSize = 1; break;
		case TextureFormat::RG8: pixelSize = 2; break;
		case TextureFormat::RGB8: pixelSize = 3; break;
		case TextureFormat::RGBA8: pixelSize = 4; break;
		case TextureFormat::R16F: case TextureFormat::RG16F: 
		case TextureFormat::RGB16F: case TextureFormat::RGBA16F:
			pixelSize = 2 * (desc.format >= TextureFormat::RGB16F ? 4 : (desc.format >= TextureFormat::RG16F ? 2 : 1));
			break;
		case TextureFormat::R32F: case TextureFormat::RG32F:
		case TextureFormat::RGB32F: case TextureFormat::RGBA32F:
			pixelSize = 4 * (desc.format >= TextureFormat::RGB32F ? 4 : (desc.format >= TextureFormat::RG32F ? 2 : 1));
			break;
		case TextureFormat::D16: pixelSize = 2; break;
		case TextureFormat::D24: case TextureFormat::D32F: pixelSize = 4; break;
		case TextureFormat::D24S8: pixelSize = 4; break;
	}

	size_t totalSize = 0;
	int w = desc.width, h = desc.height, d = desc.depth;
	for (int i = 0; i < desc.mipLevels; ++i) {
		totalSize += size_t(w) * h * d * pixelSize * desc.arrayLayers;
		w = std::max(1, w / 2);
		h = std::max(1, h / 2);
		d = std::max(1, d / 2);
	}
	data.Alloc(totalSize);
	dataSize = totalSize;
}

void TextureCPU::UpdateData(void* data, size_t size, int mipLevel, int arrayLayer) {
	// Simple implementation - just replace all data
	if (size <= dataSize) {
		memcpy(~this->data, data, size);
	}
}

Texture* sgCreateTexture(Device* device, const TextureDesc& desc) {
	return device->CreateTexture(desc);
}

void sgDestroyTexture(Texture* texture) {
	delete texture;
}

} // namespace SoftGraphics