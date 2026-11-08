#pragma once

#include "SoftGraphics.h"

namespace SoftGraphics {

class Texture {
public:
	Texture(const TextureDesc& desc) : desc(desc) {}
	virtual ~Texture() = default;

	const TextureDesc& GetDesc() const { return desc; }
	virtual void UpdateData(void* data, size_t size, int mipLevel = 0, int arrayLayer = 0) = 0;
	virtual void* GetData() const = 0;

protected:
	TextureDesc desc;
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;
};

class TextureCPU : public Texture {
public:
	TextureCPU(const TextureDesc& desc);
	~TextureCPU() override = default;

	void UpdateData(void* data, size_t size, int mipLevel = 0, int arrayLayer = 0) override;
	void* GetData() const override { return const_cast<byte*>(~data); }

private:
	Upp::Buffer<byte> data;
	size_t dataSize = 0;
};

} // namespace SoftGraphics