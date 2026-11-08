#ifndef _ComputerVision_OpticalFlowLK_h_
#define _ComputerVision_OpticalFlowLK_h_

class OpticalFlowLK {
	
public:
	
	void Track(const pyra8& prev_pyr, pyra8& curr_pyr,
		const Vector<float>& prev_xy, Vector<float>& curr_xy, int count,
		int win_size, int max_iter, Vector<uint8>& status,
		double eps = 0.01, double min_eigen_threshold = 0.0001);
	
};

#endif
