#include "SoftHMD.h"

NAMESPACE_UPP

INITBLOCK_(SoftHMD) {
	::Upp::DaemonBase::Register<Upp::HMD::SoftHMDService>("SoftHMD");
}

END_UPP_NAMESPACE
