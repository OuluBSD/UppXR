#ifndef _ComputerVision_Backend_h_
#define _ComputerVision_Backend_h_

enum class CvBackend {
	CPU = 0,
	AMP,
	OGL,
};

CvBackend GetCvBackend();
void SetCvBackend(CvBackend backend);
CvBackend ResolveCvBackend(CvBackend backend);

bool IsCvBackendSupported(CvBackend backend);
String GetCvBackendName(CvBackend backend);
String GetCvBackendStatus(CvBackend backend);

#endif
