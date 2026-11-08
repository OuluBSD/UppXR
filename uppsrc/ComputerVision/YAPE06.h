#ifndef _ComputerVision_YAPE06_h_
#define _ComputerVision_YAPE06_h_


class Yape06 {
	Vector<int> laplacian;
	
public:
	int laplacian_threshold = 30;
	int min_eigen_value_threshold = 25;
	
	
	void ComputeLaplacian(const Vector<byte>& src, Vector<int>& dst, int w, int h, int Dxx, int Dyy, int sx, int sy, int ex, int ey);
	int HessianMinEigenValue(const Vector<byte>& src, int off, double tr, int Dxx, int Dyy, int Dxy, int Dyx);
	int Detect(const ByteMat& src, Vector<Keypoint>& points, int border=5);
	int Detect(const ByteMat& src, const Rect& roi, Vector<Keypoint>& points, int border=5);
	
};


#endif
