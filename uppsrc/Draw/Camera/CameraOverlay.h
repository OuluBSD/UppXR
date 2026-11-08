#ifndef _Draw_Camera_CameraOverlay_h_
#define _Draw_Camera_CameraOverlay_h_

struct StereoTrackerStatsData {
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

struct StereoOverlayData {
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

#endif
