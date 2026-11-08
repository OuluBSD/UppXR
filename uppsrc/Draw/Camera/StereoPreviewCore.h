#ifndef _Draw_Camera_StereoPreviewCore_h_
#define _Draw_Camera_StereoPreviewCore_h_

struct StereoPreviewSettings : Moveable<StereoPreviewSettings> {
	bool overlay_eyes = false;
	bool overlay_swap = false;
	bool tint_overlay = false;
	bool rectified_overlay = false;
	bool show_difference = false;
	bool show_epipolar = false;
	bool show_crosshair = false;
	float overlay_alpha = 0.5f;   // 0..1
	float processing_scale = 1.0f; // 1.0 = full res
};

struct StereoPreviewOutput : Moveable<StereoPreviewOutput> {
	Image left;
	Image right;
	bool overlay_mode = false;
};

struct StereoPreviewLensParams {
	float f = 0;
	float cx = 0, cy = 0;
	float k1 = 0, k2 = 0;
};

class StereoPreviewEngine {
	StereoPreviewSettings settings;
	StereoCalibrationData calib;
	bool has_calib = false;

public:
	void SetSettings(const StereoPreviewSettings& s) { settings = s; }
	const StereoPreviewSettings& GetSettings() const { return settings; }

	void SetCalibration(const StereoCalibrationData& data);
	void ClearCalibration() { has_calib = false; }
	bool HasCalibration() const { return has_calib; }

	void Process(const Image& left_in, const Image& right_in, StereoPreviewOutput& out);

private:
	static StereoPreviewLensParams BuildLensParams(const StereoCalibrationData& data, const Size& sz);
	static Image ApplyTintGreen(const Image& src);
	static Image ApplyTintViolet(const Image& src);
	static Image BlendImages(const Image& a, const Image& b, float alpha);
	static Image ComputeDiff(const Image& a, const Image& b);
	static Image DrawCrosshair(const Image& src);
	static Image DrawEpipolar(const Image& src, int step = 40);
	static Image ApplyProcessingScale(const Image& src, float scale);
};

Image RectifyAndRotateOnePass(const Image& src, const StereoPreviewLensParams& lp, float yaw, float pitch, float roll, Size out_sz);
Pointf ProjectPointOnePass(Pointf src_norm, Size src_sz, const StereoPreviewLensParams& lp, float yaw, float pitch, float roll);
Pointf UnprojectPointOnePass(Pointf rect_px, Size rect_sz, const StereoPreviewLensParams& lp, float yaw, float pitch, float roll);

#endif
