#include "Surface.h"

namespace SoftGraphics {

SurfaceCPU::SurfaceCPU(void* windowHandle) : windowHandle(windowHandle) {}

void SurfaceCPU::Resize(int width, int height) {
	// No-op for CPU backend
}

void SurfaceCPU::Present() {
	// No-op for CPU backend
}

Surface* sgCreateSurface(Device* device, void* windowHandle) {
	return new SurfaceCPU(windowHandle);
}

void sgDestroySurface(Surface* surface) {
	delete surface;
}

} // namespace SoftGraphics