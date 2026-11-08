#ifndef _SoftHMD_Tracking_h_
#define _SoftHMD_Tracking_h_

NAMESPACE_HMD_BEGIN

struct StereoTrackerStats {
	int frame_count = 0;
	int processed_frames = 0;
	int skipped_frames = 0;
	int last_left_keypoints = 0;
	int last_right_keypoints = 0;
	int last_tracked_points = 0;
	int last_tracked_triangles = 0;
	int last_stereo_matches = 0;
	int last_process_usecs = 0;
	bool has_pose = false;
};

struct StereoOverlay {
	Size left_size;
	Size right_size;
	Vector<vec2> left_points;
	Vector<vec2> right_points;
	Vector<vec2> match_left;
	Vector<vec2> match_right;
	
	void Clear() {
		left_size.Clear();
		right_size.Clear();
		left_points.Clear();
		right_points.Clear();
		match_left.Clear();
		match_right.Clear();
	}
};

using ::Upp::StereoCalibrationData;

class StereoTracker {
public:
	HMD_APIENTRYDLL StereoTracker();
	HMD_APIENTRYDLL void Reset();
	HMD_APIENTRYDLL void SetWmrDefaults();
	HMD_APIENTRYDLL void SetWmrDefaults(int vendor_id, int product_id);
	HMD_APIENTRYDLL void SetPointLimit(int limit);
	HMD_APIENTRYDLL void SetDistanceLimit(int limit);
	HMD_APIENTRYDLL void SetUseBright(bool b);
	HMD_APIENTRYDLL void SetUseDark(bool b);
	HMD_APIENTRYDLL void SetSplitStereo(bool b);

	HMD_APIENTRYDLL bool PushFrame(const Image& frame, bool is_bright);

	HMD_APIENTRYDLL StereoTrackerStats GetStats() const;
	HMD_APIENTRYDLL vec3 GetPosition() const;
	HMD_APIENTRYDLL quat GetOrientation() const;
	HMD_APIENTRYDLL bool HasPose() const;
	HMD_APIENTRYDLL const Octree* GetPointcloud() const;
	HMD_APIENTRYDLL bool GetOverlay(StereoOverlay& out) const;
	HMD_APIENTRYDLL void SetCalibration(const StereoCalibrationData& data);
	HMD_APIENTRYDLL StereoCalibrationData GetCalibration() const;
	
	HMD_APIENTRYDLL static bool LoadCalibrationFile(const String& path, StereoCalibrationData& out);
	HMD_APIENTRYDLL static bool SaveCalibrationFile(const String& path, const StereoCalibrationData& data);

private:
	bool SplitStereo(const Image& frame, Image& left, Image& right) const;
	void InitOctrees();
	void ClearFrame(UncameraFrame& frame);

	mutable Upp::Mutex mutex;
	VirtualStereoUncamera uncam;
	OrbSystem orb;
	UncameraFrame frames[2];
	int write_index = 0;
	int latest_index = -1;
	bool has_prev = false;
	bool use_bright = true;
	bool use_dark = false;
	bool split_stereo = true;
	int point_limit = 2048;
	bool octree_inited = false;
	StereoTrackerStats stats;
	StereoCalibrationData calib;
	vec3 position = vec3(0,0,0);
	quat orientation = Identity<quat>();
};

NAMESPACE_HMD_END

#endif
