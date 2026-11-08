#include "Instance.h"
#include "Device.h"

namespace SoftGraphics {

Instance* sgCreateInstance() {
	return new InstanceImpl();
}

void sgDestroyInstance(Instance* instance) {
	delete instance;
}

Array<Device*> sgEnumerateDevices(Instance* instance, Backend preferredBackend) {
	return instance->EnumerateDevices(preferredBackend);
}

Device* sgCreateDevice(Instance* instance, Device* physicalDevice, Backend backend) {
	if (backend == Backend::CPU) {
		return new DeviceCPU();
	}
	// TODO: Other backends
	return nullptr;
}

void sgDestroyDevice(Device* device) {
	delete device;
}

} // namespace SoftGraphics

#ifdef flagMAIN
CONSOLE_APP_MAIN {
	// SoftGraphics main entry point for TheIDE startup dialog
}
#endif