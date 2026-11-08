#ifndef _ComputerVision_ORB_h_
#define _ComputerVision_ORB_h_


struct KeypointMatch : Moveable<KeypointMatch> {
    int screen_idx;
    int pattern_lev;
    int pattern_idx;
    int distance;
    
    void Set(int screen_idx=0, int pattern_lev=0, int pattern_idx=0, int distance=0);
};


class Orb {
	
public:
	static const int bit_pattern_31_[];
	
	FloatMat	H;
	ByteMat		patch_img;
	CvBackend	last_backend = CvBackend::CPU;
	double		last_describe_ms = 0;
	double		last_cpu_ms = 0;
	double		last_amp_ms = 0;
	double		last_ogl_ms = 0;
	
	
	Orb();
	void RectifyPatch(const ByteMat& src, ByteMat& dst, double angle, int px, int py, int psize);
	void Describe(const ByteMat& src, const Vector<Keypoint>& corners, Vector<BinDescriptor>& descriptors);
	void DescribeCpu(const ByteMat& src, const Vector<Keypoint>& corners, Vector<BinDescriptor>& descriptors);
	void DescribeAmp(const ByteMat& src, const Vector<Keypoint>& corners, Vector<BinDescriptor>& descriptors);
	void DescribeOglStub(const ByteMat& src, const Vector<Keypoint>& corners, Vector<BinDescriptor>& descriptors);
	CvBackend GetLastBackend() const { return last_backend; }
	double GetLastDescribeMs() const { return last_describe_ms; }
	double GetLastCpuMs() const { return last_cpu_ms; }
	double GetLastAmpMs() const { return last_amp_ms; }
	double GetLastOglMs() const { return last_ogl_ms; }
	
};


#endif
