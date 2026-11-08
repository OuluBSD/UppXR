#pragma once

#include "SoftGraphics.h"

namespace SoftGraphics {

class Buffer {
public:
	Buffer(const BufferDesc& desc) : desc(desc) {}
	virtual ~Buffer() = default;

	const BufferDesc& GetDesc() const { return desc; }
	virtual void* Map() = 0;
	virtual void Unmap() = 0;
	virtual void UpdateData(void* data, size_t size, size_t offset = 0) = 0;

protected:
	BufferDesc desc;
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer&&) = default;
	Buffer& operator=(Buffer&&) = default;
};

class BufferCPU : public Buffer {
public:
	BufferCPU(const BufferDesc& desc);
	~BufferCPU() override = default;

	void* Map() override;
	void Unmap() override;
	void UpdateData(void* data, size_t size, size_t offset = 0) override;

	const Upp::byte* GetData() const { return dataSize ? (const Upp::byte*)~data : nullptr; }
	size_t GetSize() const { return dataSize; }

private:
	Upp::Buffer<byte> data;
	size_t dataSize = 0;
	bool mapped = false;
};

} // namespace SoftGraphics