#ifndef _ComputerVision_OrbSystem_h_
#define _ComputerVision_OrbSystem_h_

struct ColorLine : Moveable<ColorLine> {
	Point a, b;
	Color clr;
	byte alpha = 255;
};

class OrbSystem {
public:
        const Vector<BinDescriptor>& GetPatternDescriptors(int lev) const { return pattern_descriptors[lev]; }
	struct Profile : Moveable<Profile> {
		int64 total_us = 0;
		int64 grayscale_us = 0;
		int64 blur_us = 0;
		int64 detect_us = 0;
		int64 describe_us = 0;
		int64 render_corners_us = 0;
		int64 match_us = 0;
		int64 transform_us = 0;
		int64 render_matches_us = 0;
		int64 render_shape_us = 0;
		int num_corners = 0;
		int num_matches = 0;
		int good_matches = 0;
		
		void Clear() {
			total_us = 0;
			grayscale_us = 0;
			blur_us = 0;
			detect_us = 0;
			describe_us = 0;
			render_corners_us = 0;
			match_us = 0;
			transform_us = 0;
			render_matches_us = 0;
			render_shape_us = 0;
			num_corners = 0;
			num_matches = 0;
			good_matches = 0;
		}
	};

private:
	DMatrix<byte> lev0_img;
	DMatrix<byte> lev_img;
	DMatrix<byte> pattern_preview;
	DMatrix<byte> match_mask;
	DMatrix<byte> train_img;
	DMatrix<float> homo3x3;
	Vector<Keypoint> screen_corners;
	
	Vector<BinDescriptor> screen_descriptors;
	Vector<Vector<BinDescriptor>> pattern_descriptors;
	
	Vector<Vector<Keypoint>> pattern_corners;
	Vector<Keypoint> lev_corners, lev_descr;
	Vector<Keypoint> corners, pattern_xy, screen_xy;
	Vector<KeypointMatch> matches;
	
	int num_train_levels = 10;
	Orb o;
	Yape06 y;
	Homography2D mm_kernel;
	RansacParams ransac_param;
	MotionEstimator<Homography2D> mot;
	
	int    blur_size = 9;
	int    lap_thres = 2;
	int    eigen_thres = 2;
	int    keypoint_match_threshold = 95;
	double ratio_test = 0.8;
	bool   exact_scale_only = false;
	bool   use_orientation = true;
	bool   render_debug = true;
	bool   use_ransac = true;
	
	Size   pattern_sz;
	
	void render_matches(const Vector<KeypointMatch>& matches);
	void render_pattern_shape();
	void RenderCorners(const ByteMat& bg, const ByteMat* mini_img, const Vector<Keypoint>& corners, ByteMat& out);
	void OutputFromGray(const ByteMat& gray);
	
public:
	static const int u_max[];

        struct GpuKp : Moveable<GpuKp> { float x, y, score; int level; GpuKp() {} GpuKp(float x, float y, float score, int level) : x(x), y(y), score(score), level(level) {} };

	void SetInput(Image i);
	void InitDefault();
	void Process();
	void ProcessROI(Rect roi);
        void ProcessGpu(const ByteMat& gray, const Vector<GpuKp>& keypoints, int level, Rect roi);
        void ProcessGpu(const Vector<GpuKp>& keypoints, const Vector<BinDescriptor>& descriptors, Rect roi);
	void ProcessPrepared(const ByteMat& gray, const ByteMat& smooth, Rect roi);
	
	void TrainPattern();
	int  DetectKeypoints(DescriptorImage& output, int max_allowed);
	int  DetectKeypointsROI(Rect roi, DescriptorImage& output, int max_allowed);
	int  DetectKeypoints(const ByteMat& img, Vector<Keypoint>& corners, int max_allowed);
	int  DetectKeypoints(const ByteMat& img, const Rect& roi, Vector<Keypoint>& corners, int max_allowed);
	double IcAngle(const ByteMat& img, int px, int py);
	int  FindTransform(Vector<KeypointMatch>& matches);
	int  MatchPattern();
	void TCorners(const Vector<float>& M, int w, int h);

	void SetKeypointMatchThreshold(int t) { keypoint_match_threshold = t; }
	int  GetKeypointMatchThreshold() const { return keypoint_match_threshold; }
	void SetExactScaleOnly(bool b)        { exact_scale_only = b; }
	void SetUseOrientation(bool b)        { use_orientation = b; }
	void SetRenderDebug(bool b)           { render_debug = b; }
	void SetUseRansac(bool b)             { use_ransac = b; }
	
	void SetRatioTest(double r)           { ratio_test = r; }
	double GetRatioTest() const             { return ratio_test; }

	int  GetLastGoodMatches() const       { return last_good_matches; }
	int  GetLastMatchCount() const        { return last_match_count; }
	const Vector<Pointf>& GetLastCorners() const { return last_corners; }
	const Vector<float>& GetHomo() const  { return homo3x3.data; }
	
	ByteMat input, output, tmp0, tmp1, tmp2;
	Size sz;
	Vector<ColorLine> lines;

	Vector<Pointf> last_corners;
	int last_good_matches = 0;
	int last_match_count = 0;
	Profile last_profile;

public:
	const Profile& GetLastProfile() const { return last_profile; }
};

#endif
