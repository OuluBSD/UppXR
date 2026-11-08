#include "Buffer.h"
#include "Device.h"

namespace SoftGraphics {

BufferCPU::BufferCPU(const BufferDesc& desc) : Buffer(desc) {
	data.Alloc(desc.size);
	dataSize = desc.size;
	if (desc.initialData && desc.size > 0) {
		memcpy(~data, desc.initialData, desc.size);
	}
}

void* BufferCPU::Map() {
	mapped = true;
	return ~data;
}

void BufferCPU::Unmap() {
	mapped = false;
}

void BufferCPU::UpdateData(void* data, size_t size, size_t offset) {
	if (offset + size <= dataSize) {
		memcpy(~this->data + offset, data, size);
	}
}

Buffer* sgCreateBuffer(Device* device, const BufferDesc& desc) {
	return device->CreateBuffer(desc);
}

void sgDestroyBuffer(Buffer* buffer) {
	delete buffer;
}

} // namespace SoftGraphics