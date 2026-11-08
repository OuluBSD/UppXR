#include "SoftHMD.h"

NAMESPACE_HMD_BEGIN

SoftHmdVisualTracker::SoftHmdVisualTracker() {
}

void SoftHmdVisualTracker::SetWmrDefaults(int vendor_id, int product_id) {
	tracker.SetWmrDefaults(vendor_id, product_id);
}

void SoftHmdVisualTracker::Reset() {
	tracker.Reset();
	has_state = false;
	state = FusionState();
}

void SoftHmdVisualTracker::SetUseBright(bool b) {
	tracker.SetUseBright(b);
}

void SoftHmdVisualTracker::SetUseDark(bool b) {
	tracker.SetUseDark(b);
}

void SoftHmdVisualTracker::SetSplitStereo(bool b) {
	tracker.SetSplitStereo(b);
}

void SoftHmdVisualTracker::SetPointLimit(int limit) {
	tracker.SetPointLimit(limit);
}

void SoftHmdVisualTracker::SetDistanceLimit(int limit) {
	tracker.SetDistanceLimit(limit);
}

StereoTrackerStats SoftHmdVisualTracker::GetStats() const {
	return tracker.GetStats();
}

const Octree* SoftHmdVisualTracker::GetPointcloud() const {
	return tracker.GetPointcloud();
}

bool SoftHmdVisualTracker::GetOverlay(StereoOverlay& out) const {
	return tracker.GetOverlay(out);
}

void SoftHmdVisualTracker::SetCalibration(const StereoCalibrationData& data) {
	tracker.SetCalibration(data);
}

StereoCalibrationData SoftHmdVisualTracker::GetCalibration() const {
	return tracker.GetCalibration();
}

bool SoftHmdVisualTracker::HasPose() const {
	return tracker.HasPose();
}

vec3 SoftHmdVisualTracker::GetPosition() const {
	return tracker.GetPosition();
}

quat SoftHmdVisualTracker::GetOrientation() const {
	return tracker.GetOrientation();
}

bool SoftHmdVisualTracker::DecodeFrame(const VisualFrame& frame, Image& out) const {
	if (!frame.img.IsEmpty()) {
		out = frame.img;
		return true;
	}
	
	if (!frame.data || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0)
		return false;
	
	Size sz(frame.width, frame.height);
	ImageBuffer ib(sz);
	
	switch (frame.format) {
		case GEOM_EVENT_CAM_LUMA8: {
			for (int y = 0; y < sz.cy; y++) {
				const byte* src = frame.data + y * frame.stride;
				RGBA* dst = ib[y];
				for (int x = 0; x < sz.cx; x++) {
					byte v = src[x];
					RGBA c;
					c.r = v;
					c.g = v;
					c.b = v;
					c.a = 255;
					dst[x] = c;
				}
			}
			break;
		}
		case GEOM_EVENT_CAM_BGRA8: {
			for (int y = 0; y < sz.cy; y++) {
				const byte* src = frame.data + y * frame.stride;
				RGBA* dst = ib[y];
				for (int x = 0; x < sz.cx; x++) {
					const byte* p = src + x * 4;
					RGBA c;
					c.r = p[2];
					c.g = p[1];
					c.b = p[0];
					c.a = p[3];
					dst[x] = c;
				}
			}
			break;
		}
		case GEOM_EVENT_CAM_RGBA8: {
			for (int y = 0; y < sz.cy; y++) {
				const byte* src = frame.data + y * frame.stride;
				RGBA* dst = ib[y];
				for (int x = 0; x < sz.cx; x++) {
					const byte* p = src + x * 4;
					RGBA c;
					c.r = p[0];
					c.g = p[1];
					c.b = p[2];
					c.a = p[3];
					dst[x] = c;
				}
			}
			break;
		}
		default:
			return false;
	}
	
	out = ib;
	return true;
}

void SoftHmdVisualTracker::PutFrame(const VisualFrame& frame) {
	Image img;
	if (!DecodeFrame(frame, img))
		return;
	
	bool is_bright = (frame.flags & VIS_FRAME_BRIGHT) != 0;
	if ((frame.flags & (VIS_FRAME_BRIGHT | VIS_FRAME_DARK)) == 0)
		is_bright = true;
	
	if (!tracker.PushFrame(img, is_bright))
		return;
	
	if (tracker.HasPose()) {
		state.timestamp_us = frame.timestamp_us;
		state.position = tracker.GetPosition();
		state.orientation = tracker.GetOrientation();
		has_state = true;
	}
}

bool SoftHmdVisualTracker::GetState(FusionState& out) const {
	if (!has_state)
		return false;
	out = state;
	return true;
}


void SoftHmdImuTracker::Reset() {
	has_sample = false;
	last_sample = ImuSample();
}

void SoftHmdImuTracker::PutSample(const ImuSample& sample) {
	last_sample = sample;
	has_sample = true;
}


void SoftHmdFusion::Reset() {
	visual_bright.Reset();
	visual_dark.Reset();
	imu.Reset();
	has_state = false;
	state = FusionState();
	has_prev_dark = false;
	has_last_dark = false;
}

void SoftHmdFusion::PutVisual(const VisualFrame& frame) {
	bool is_bright = (frame.flags & VIS_FRAME_BRIGHT) != 0;
	bool is_dark = (frame.flags & VIS_FRAME_DARK) != 0;
	if (!is_bright && !is_dark)
		is_bright = true;
	
	if (is_bright)
		visual_bright.PutFrame(frame);
	if (is_dark)
		visual_dark.PutFrame(frame);
	
	PoseSample sample;
	if (is_dark && GetTrackerState(false, sample)) {
		prev_dark = last_dark;
		has_prev_dark = has_last_dark;
		last_dark = sample;
		has_last_dark = true;
		state.timestamp_us = sample.timestamp_us;
		state.position = sample.position;
		state.orientation = sample.orientation;
		has_state = true;
	}
	else if (is_bright && GetTrackerState(true, sample)) {
		state.timestamp_us = sample.timestamp_us;
		state.position = sample.position;
		state.orientation = sample.orientation;
		has_state = true;
		ApplyInterleavedConstraint(true);
	}
}

void SoftHmdFusion::PutImu(const ImuSample& sample) {
	imu.PutSample(sample);
}

bool SoftHmdFusion::GetState(FusionState& out) const {
	if (!has_state)
		return false;
	out = state;
	return true;
}

bool SoftHmdFusion::GetTrackerState(bool is_bright, PoseSample& out) const {
	FusionState st;
	if (is_bright) {
		if (!visual_bright.GetState(st))
			return false;
	}
	else {
		if (!visual_dark.GetState(st))
			return false;
	}
	out.timestamp_us = st.timestamp_us;
	out.position = st.position;
	out.orientation = st.orientation;
	return true;
}

void SoftHmdFusion::ApplyInterleavedConstraint(bool is_bright) {
	if (!is_bright)
		return;
	if (!has_prev_dark || !has_last_dark)
		return;
	if (state.timestamp_us <= prev_dark.timestamp_us || state.timestamp_us >= last_dark.timestamp_us)
		return;
	
	double denom = (double)(last_dark.timestamp_us - prev_dark.timestamp_us);
	if (denom <= 0)
		return;
	double t = (double)(state.timestamp_us - prev_dark.timestamp_us) / denom;
	vec3 interp_pos = Mix(prev_dark.position, last_dark.position, (float)t);
	quat interp_orient = Slerp(prev_dark.orientation, last_dark.orientation, (float)t);
	
	state.position = Mix(state.position, interp_pos, 0.5f);
	state.orientation = Slerp(state.orientation, interp_orient, 0.5f);
}

NAMESPACE_HMD_END
