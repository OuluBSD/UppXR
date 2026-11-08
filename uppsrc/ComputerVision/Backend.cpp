#include "ComputerVision.h"

NAMESPACE_UPP

namespace {
	CvBackend backend =
	#ifdef flagAMPCOMPAT
		CvBackend::AMP;
	#else
		CvBackend::CPU;
	#endif
}

CvBackend GetCvBackend() {
	return backend;
}

void SetCvBackend(CvBackend b) {
	backend = b;
}

CvBackend ResolveCvBackend(CvBackend b) {
	return IsCvBackendSupported(b) ? b : CvBackend::CPU;
}

bool IsCvBackendSupported(CvBackend b) {
	switch (b) {
	case CvBackend::CPU:
		return true;
	case CvBackend::AMP:
		return true;
	case CvBackend::OGL:
		return false; // planned stub only for now
	}
	return false;
}

String GetCvBackendName(CvBackend b) {
	switch (b) {
	case CvBackend::CPU:
		return "CPU";
	case CvBackend::AMP:
		return "AMP";
	case CvBackend::OGL:
		return "OpenGL";
	}
	return "Unknown";
}

String GetCvBackendStatus(CvBackend b) {
	if (!IsCvBackendSupported(b))
		return GetCvBackendName(b) + " (stub: not implemented yet)";
	if (b == CvBackend::AMP) {
		#ifdef flagAMPCOMPAT
		return "AMP (compat mode)";
		#else
		return "AMP (native mode)";
		#endif
	}
	return GetCvBackendName(b);
}

END_UPP_NAMESPACE
