#ifndef _SoftHMD_TrackingImpl_h_
#define _SoftHMD_TrackingImpl_h_

NAMESPACE_HMD_BEGIN

class SoftHmdVisualTracker : public ::Upp::VisualTracker {
public:
	SoftHmdVisualTracker();
	void Reset() override;
	void PutFrame(const VisualFrame& frame) override;
	bool GetState(FusionState& out) const override;
	void SetWmrDefaults(int vendor_id = 0, int product_id = 0) override;
	
	void SetUseBright(bool b);
	void SetUseDark(bool b);
	void SetSplitStereo(bool b);
	void SetPointLimit(int limit);
	void SetDistanceLimit(int limit);
	StereoTrackerStats GetStats() const;
	const Octree* GetPointcloud() const;
	bool GetOverlay(StereoOverlay& out) const;
	void SetCalibration(const StereoCalibrationData& data);
	StereoCalibrationData GetCalibration() const;
	bool HasPose() const;
	vec3 GetPosition() const;
	quat GetOrientation() const;
	
private:
	bool DecodeFrame(const VisualFrame& frame, Image& out) const;
	
	StereoTracker tracker;
	FusionState state;
	bool has_state = false;
};

class SoftHmdImuTracker : public ::Upp::ImuTracker {
public:
	void Reset() override;
	void PutSample(const ImuSample& sample) override;
	
	const ImuSample& GetLastSample() const { return last_sample; }
	bool HasSample() const { return has_sample; }
	
private:
	ImuSample last_sample;
	bool has_sample = false;
};

class SoftHmdFusion : public ::Upp::Fusion {
public:
	void Reset() override;
	void PutVisual(const VisualFrame& frame) override;
	void PutImu(const ImuSample& sample) override;
	bool GetState(FusionState& out) const override;
	
	SoftHmdVisualTracker& GetBrightTracker() { return visual_bright; }
	SoftHmdVisualTracker& GetDarkTracker() { return visual_dark; }
	SoftHmdImuTracker& GetImu() { return imu; }
	
private:
	struct PoseSample {
		int64 timestamp_us = 0;
		vec3 position = vec3(0,0,0);
		quat orientation;
	};
	
	void ApplyInterleavedConstraint(bool is_bright);
	bool GetTrackerState(bool is_bright, PoseSample& out) const;
	
	SoftHmdVisualTracker visual_bright;
	SoftHmdVisualTracker visual_dark;
	SoftHmdImuTracker imu;
	FusionState state;
	bool has_state = false;
	PoseSample prev_dark;
	PoseSample last_dark;
	bool has_prev_dark = false;
	bool has_last_dark = false;
};

class SoftHmdRelocalizer : public ::Upp::Relocalizer {
public:
	void Reset() override {}
	bool Relocalize(const VisualFrame&, FusionState&) override { return false; }
};

NAMESPACE_HMD_END

#endif
