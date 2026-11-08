#ifndef _ComputerVision_GaussMotionFilter_h_
#define _ComputerVision_GaussMotionFilter_h_


class GaussMotionFilter {
	int radius = 15;
	double stdev = -1.0;
	
	Vector<float> weight;
	DMatrix<float> m33;
	
public:
	
	void Setup(int radius=15, double stdev=-1.0);
	FloatMat& Stabilize(int idx, Vector<FloatMat>& motions, int from_idx, int to_idx);
	
};


#endif
