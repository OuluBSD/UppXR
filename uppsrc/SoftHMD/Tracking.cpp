#include "SoftHMD.h"

NAMESPACE_HMD_BEGIN

StereoTracker::StereoTracker() {
	orb.InitDefault();
	Reset();
}

void StereoTracker::Reset() {
	Upp::Mutex::Lock __(mutex);
	stats = StereoTrackerStats();
	position = vec3(0,0,0);
	orientation = Identity<quat>();
	write_index = 0;
	latest_index = -1;
	has_prev = false;
	octree_inited = false;
	frames[0].tracked_points.Clear();
	frames[0].tracked_triangles.Clear();
	frames[1].tracked_points.Clear();
	frames[1].tracked_triangles.Clear();
}

void StereoTracker::SetWmrDefaults() {
	SetWmrDefaults(0, 0);
}

void StereoTracker::SetWmrDefaults(int vendor_id, int product_id) {
	Upp::Mutex::Lock __(mutex);
	
	// Default WMR/HoloLens values
	float a = 17.4932f, b = 153.022f, c = 175.333f, d = -25.7489f;
	float eye_dist = 0.12f;
	float outward_angle = DEG2RAD(35.50f);
	float right_pitch = 0;
	float right_roll = 0;
	vec2 principal_point(0, 0);
	
	bool loaded = false;
	if (vendor_id == 0x03f0 || vendor_id == 0x04b4) {
		String path = "share/calibration/hp_vr1000/calibration.stcal";
		StereoCalibrationData cal;
		if (LoadCalibrationFile(path, cal)) {
			a = cal.angle_to_pixel[0];
			b = cal.angle_to_pixel[1];
			c = cal.angle_to_pixel[2];
			d = cal.angle_to_pixel[3];
			eye_dist = cal.eye_dist / 1000.0f; // mm to m
			outward_angle = cal.outward_angle;
			right_pitch = cal.right_pitch;
			right_roll = cal.right_roll;
			principal_point = cal.principal_point;
			loaded = true;
			LOG("StereoTracker: Loaded calibration from " << path);
		}
	}

	uncam.SetAnglePixel(a, b, c, d);
	uncam.SetEyeDistance(eye_dist);
	uncam.SetYLevelHeight(10);
	uncam.SetEyeOutwardAngle(outward_angle);
	uncam.SetRightTilt(right_pitch, right_roll);
	if (principal_point[0] > 0 && principal_point[1] > 0)
		uncam.SetPrincipalPoint(principal_point[0], principal_point[1]);
	uncam.SetDistanceLimit(128);

	calib.is_enabled = true;
	calib.angle_to_pixel = vec4(a, b, c, d);
	calib.eye_dist = eye_dist;  // Already in meters
	calib.outward_angle = outward_angle;
	calib.right_pitch = right_pitch;
	calib.right_roll = right_roll;
	calib.principal_point = principal_point;
}

bool StereoTracker::LoadCalibrationFile(const String& path, StereoCalibrationData& out) {
	return VisitFromJsonFile(out, path);
}

bool StereoTracker::SaveCalibrationFile(const String& path, const StereoCalibrationData& data) {
	return VisitToJsonFile(data, path);
}

void StereoTracker::SetPointLimit(int limit) {
	Upp::Mutex::Lock __(mutex);
	point_limit = limit;
}

void StereoTracker::SetDistanceLimit(int limit) {
	Upp::Mutex::Lock __(mutex);
	uncam.SetDistanceLimit(limit);
}

void StereoTracker::SetUseBright(bool b) {
	Upp::Mutex::Lock __(mutex);
	use_bright = b;
}

void StereoTracker::SetUseDark(bool b) {
	Upp::Mutex::Lock __(mutex);
	use_dark = b;
}

void StereoTracker::SetSplitStereo(bool b) {
	Upp::Mutex::Lock __(mutex);
	split_stereo = b;
}

StereoTrackerStats StereoTracker::GetStats() const {
	Upp::Mutex::Lock __(mutex);
	return stats;
}

vec3 StereoTracker::GetPosition() const {
	Upp::Mutex::Lock __(mutex);
	return position;
}

quat StereoTracker::GetOrientation() const {
	Upp::Mutex::Lock __(mutex);
	return orientation;
}

bool StereoTracker::HasPose() const {
	Upp::Mutex::Lock __(mutex);
	return stats.has_pose;
}

const Octree* StereoTracker::GetPointcloud() const {
	Upp::Mutex::Lock __(mutex);
	if (latest_index < 0)
		return 0;
	return &frames[latest_index].otree;
}

bool StereoTracker::GetOverlay(StereoOverlay& out) const {
	Upp::Mutex::Lock __(mutex);
	if (latest_index < 0)
		return false;
	const UncameraFrame& frame = frames[latest_index];
	out.left_size = frame.l_dimg.GetResolution();
	out.right_size = frame.r_dimg.GetResolution();
	out.left_points.Clear();
	out.right_points.Clear();
	out.match_left.Clear();
	out.match_right.Clear();

	const Vector<Descriptor32>& left_desc = frame.l_dimg.GetDescriptors();
	out.left_points.SetCount(left_desc.GetCount());
	for (int i = 0; i < left_desc.GetCount(); i++) {
		const Descriptor32& d = left_desc[i];
		out.left_points[i] = vec2(d.x, d.y);
	}

	const Vector<Descriptor32>& right_desc = frame.r_dimg.GetDescriptors();
	out.right_points.SetCount(right_desc.GetCount());
	for (int i = 0; i < right_desc.GetCount(); i++) {
		const Descriptor32& d = right_desc[i];
		out.right_points[i] = vec2(d.x, d.y);
	}

	for (const HorizontalMatch& match : frame.horz_match) {
		if (!match.l || !match.r)
			continue;
		out.match_left.Add(vec2(match.l->x, match.l->y));
		out.match_right.Add(vec2(match.r->x, match.r->y));
	}
	return true;
}

void StereoTracker::SetCalibration(const StereoCalibrationData& data) {
	Upp::Mutex::Lock __(mutex);
	calib = data;
	if (!calib.is_enabled)
		return;
	uncam.SetAnglePixel(calib.angle_to_pixel[0], calib.angle_to_pixel[1],
	                    calib.angle_to_pixel[2], calib.angle_to_pixel[3]);
	uncam.SetEyeOutwardAngle(calib.outward_angle);
	uncam.SetRightTilt(calib.right_pitch, calib.right_roll);
	if (calib.principal_point[0] > 0 && calib.principal_point[1] > 0)
		uncam.SetPrincipalPoint(calib.principal_point[0], calib.principal_point[1]);
	if (calib.eye_dist > 0)
		uncam.SetEyeDistance(calib.eye_dist);  // Already in meters
}

StereoCalibrationData StereoTracker::GetCalibration() const {
	Upp::Mutex::Lock __(mutex);
	return calib;
}

bool StereoTracker::SplitStereo(const Image& frame, Image& left, Image& right) const {
	Size sz = frame.GetSize();
	if (sz.cx < 4 || sz.cy < 4)
		return false;
	if (!split_stereo) {
		left = frame;
		right = frame;
		return true;
	}
	if (sz.cx & 1)
		return false;
	Size half(sz.cx / 2, sz.cy);
	left = Crop(frame, RectC(0, 0, half.cx, half.cy));
	right = Crop(frame, RectC(half.cx, 0, half.cx, half.cy));
	return true;
}

void StereoTracker::InitOctrees() {
	if (octree_inited)
		return;
	frames[0].otree.Initialize(-3, 8);
	frames[1].otree.Initialize(-3, 8);
	octree_inited = true;
}

void StereoTracker::ClearFrame(UncameraFrame& frame) {
	frame.tracked_points.Clear();
	frame.tracked_triangles.Clear();
	frame.horz_match.Clear();
	frame.l_desc.Clear();
	frame.r_desc.Clear();
}

bool StereoTracker::PushFrame(const Image& frame, bool is_bright) {
	if (frame.IsEmpty())
		return false;

	Upp::Mutex::Lock __(mutex);
	stats.frame_count++;

	if ((is_bright && !use_bright) || (!is_bright && !use_dark)) {
		stats.skipped_frames++;
		return false;
	}

	Image left, right;
	if (!SplitStereo(frame, left, right)) {
		stats.skipped_frames++;
		return false;
	}

	InitOctrees();

	int64 start_usecs = usecs();
	UncameraFrame& cur = frames[write_index];
	UncameraFrame& prev = frames[1 - write_index];

	ClearFrame(cur);
	cur.l_img = left;
	cur.r_img = right;
	cur.l_dimg.SetResolution(left.GetSize());
	cur.r_dimg.SetResolution(right.GetSize());

	orb.SetInput(left);
	stats.last_left_keypoints = orb.DetectKeypoints(cur.l_dimg, point_limit);
	orb.SetInput(right);
	stats.last_right_keypoints = orb.DetectKeypoints(cur.r_dimg, point_limit);

	uncam.StageStereoKeypoints(prev, cur);
	uncam.ProcessTriangles(cur);
	if (has_prev) {
		uncam.SolveTransform(prev, cur);
		uncam.StageProcessTransform(prev, cur);
		position = cur.position;
		orientation = cur.orientation;
		stats.has_pose = true;
	}

	stats.last_tracked_points = cur.tracked_points.GetCount();
	stats.last_tracked_triangles = cur.tracked_triangles.GetCount();
	stats.last_stereo_matches = cur.horz_match.GetCount();
	stats.last_process_usecs = (int)(usecs() - start_usecs);
	stats.processed_frames++;

	latest_index = write_index;
	write_index = 1 - write_index;
	has_prev = true;

	return true;
}

NAMESPACE_HMD_END
