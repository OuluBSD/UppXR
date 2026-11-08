#pragma once

#include "Device.h"

namespace SoftGraphics {

class InstanceImpl : public Instance {
public:
	InstanceImpl() = default;
	~InstanceImpl() override = default;

	Array<Device*> EnumerateDevices(Backend preferredBackend = Backend::CPU) override;
};

} // namespace SoftGraphics