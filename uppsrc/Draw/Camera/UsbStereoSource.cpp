#include "Camera.h"

NAMESPACE_UPP

namespace {

class UsbStereoSource : public StereoSource {
public:
	String device_path;
	bool running = false;
	bool verbose = false;
	int64 serial = 0;
	bool last_valid = false;
	Image last_left;
	Image last_right;
	int64 last_timestamp_us = 0;

#ifdef flagLINUX
	VideoV4L2Backend backend;
	Vector<VideoFrame> frames;
	VideoPixelFormat fmt = VID_PIX_UNKNOWN;
	Size size;
	int fps = 0;
#endif

	UsbStereoSource() {
		device_path = "/dev/video0";
	}

	String GetName() const override { return "USB Stereo (Side-by-side)"; }

	bool Start() override {
#ifdef flagLINUX
		if (running)
			return true;
		if (device_path.IsEmpty())
			device_path = "/dev/video0";
		backend.SetDevice(device_path);
		if (!TryOpen(VID_PIX_MJPEG, Size(2560, 720), 30) &&
		    !TryOpen(VID_PIX_YUYV, Size(2560, 720), 30) &&
		    !TryOpen(VID_PIX_MJPEG, Size(1280, 720), 30) &&
		    !TryOpen(VID_PIX_YUYV, Size(1280, 720), 30)) {
			return false;
		}
		running = true;
		last_valid = false;
		return true;
#else
		return false;
#endif
	}

	void Stop() override {
#ifdef flagLINUX
		if (!running)
			return;
		backend.Close();
		running = false;
		last_valid = false;
#endif
	}

	bool IsRunning() const override { return running; }

	bool ReadFrame(CameraFrame& left, CameraFrame& right, bool prefer_bright = false) override {
		(void)prefer_bright;
#ifdef flagLINUX
		if (!running)
			return false;
		backend.PopFrames(frames);
		if (frames.IsEmpty())
			return false;
		const VideoFrame& vf = frames.Top();
		Image l, r;
		if (!SplitStereoImage(vf.img, l, r))
			return false;
		serial++;
		FillFrame(left, l, vf.timestamp_us, serial);
		FillFrame(right, r, vf.timestamp_us, serial);
		ApplyEffects(left);
		ApplyEffects(right);
		last_left = l;
		last_right = r;
		last_timestamp_us = vf.timestamp_us;
		last_valid = true;
		return true;
#else
		return false;
#endif
	}

	bool PeakFrame(CameraFrame& left, CameraFrame& right, bool prefer_bright = false) override {
		if (ReadFrame(left, right, prefer_bright))
			return true;
		if (!last_valid)
			return false;
		FillFrame(left, last_left, last_timestamp_us, serial);
		FillFrame(right, last_right, last_timestamp_us, serial);
		ApplyEffects(left);
		ApplyEffects(right);
		return true;
	}

	void SetVerbose(bool v) override { verbose = v; }

	bool SetOption(const String& key, const String& value) override {
		if (key == "device") {
			device_path = value;
			return true;
		}
		return false;
	}

	void GetStatsMap(VectorMap<String, String>& out) override {
#ifdef flagLINUX
		VideoStats stats = backend.GetStats();
		out.Add("Cam Backend", "V4L2");
		out.Add("Cam Device", device_path);
		out.Add("Cam Frame Count", IntStr(stats.frame_count));
		out.Add("Cam Dropped Frames", IntStr(stats.dropped_frames));
		if (size.cx > 0 && size.cy > 0)
			out.Add("Cam Mode", Format("%d`x%d @ %d", size.cx, size.cy, fps));
		if (fmt != VID_PIX_UNKNOWN)
			out.Add("Cam Format", FormatPixelFormat(fmt));
#endif
	}

private:
#ifdef flagLINUX
	bool TryOpen(VideoPixelFormat f, Size s, int frames_per_sec) {
		fmt = f;
		size = s;
		fps = frames_per_sec;
		backend.SetFormat(fmt, size, fps);
		if (backend.Open())
			return true;
		return false;
	}
#endif

	static void FillFrame(CameraFrame& dst, const Image& img, int64 timestamp, int64 serial_id) {
		dst.img = img;
		dst.size = img.GetSize();
		dst.timestamp_us = timestamp;
		dst.format = VID_PIX_RGBA8;
		dst.is_bright = false;
		dst.serial = serial_id;
	}

	static String FormatPixelFormat(VideoPixelFormat f) {
		switch (f) {
		case VID_PIX_MJPEG: return "MJPEG";
		case VID_PIX_YUYV: return "YUYV";
		case VID_PIX_RGBA8: return "RGBA";
		case VID_PIX_BGRA8: return "BGRA";
		case VID_PIX_RGB8: return "RGB";
		case VID_PIX_BGR8: return "BGR";
		case VID_PIX_LUMA8: return "Luma";
		default: return "Unknown";
		}
	}
};

One<StereoSource> CreateUsbStereoSource() {
	return MakeOne<UsbStereoSource>();
}

struct UsbStereoSourceReg {
	UsbStereoSourceReg() {
		RegisterStereoSource("usb", "USB Stereo (Side-by-side)", CreateUsbStereoSource, 50);
	}
};

UsbStereoSourceReg s_usb_stereo_reg;

}

END_UPP_NAMESPACE
