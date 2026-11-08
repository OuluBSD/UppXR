#pragma once

#include "SoftGraphics.h"

namespace SoftGraphics {

class Surface {
public:
	Surface() = default;
	virtual ~Surface() = default;

	virtual void* GetNativeHandle() const = 0;
	virtual void Resize(int width, int height) = 0;
	virtual void Present() = 0;

protected:
	Surface(const Surface&) = delete;
	Surface& operator=(const Surface&) = delete;
};

class SurfaceCPU : public Surface {
public:
	SurfaceCPU(void* windowHandle);
	~SurfaceCPU() override = default;

	void* GetNativeHandle() const override { return windowHandle; }
	void Resize(int width, int height) override;
	void Present() override;

private:
	void* windowHandle;
};

} // namespace SoftGraphics