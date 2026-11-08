#include "SoftHMD.h"

NAMESPACE_HMD_BEGIN


SoftHMDService::SoftHMDService() {
	
}

bool SoftHMDService::Init(String name) {
	if (!sys.Initialise())
		return false;
	
	sys.Start();
	return true;
}

void SoftHMDService::Update() {
	
}

void SoftHMDService::Stop() {
	sys.Stop();
}

void SoftHMDService::Deinit() {
	sys.Uninitialise();
}

void SoftHMDService::SetSensorCallback(Callback1<GeomEvent&> cb) {
	sys.WhenSensorEvent << cb;
}




NAMESPACE_HMD_END



