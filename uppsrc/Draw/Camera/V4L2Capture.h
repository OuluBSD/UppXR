#ifndef _Draw_Camera_V4L2Capture_h_
#define _Draw_Camera_V4L2Capture_h_

class VideoV4L2Backend : public VideoBackend {
	String device;
	VideoPixelFormat fmt = VID_PIX_UNKNOWN;
	Size size;
	int fps = 0;
	VideoStats stats;
	void* cap = nullptr;
#ifdef flagLINUX
	Vector<byte> buffer;
#endif
	bool opened = false;

public:
	VideoV4L2Backend() = default;

	void SetDevice(const String& path) { device = path; }
	const String& GetDevice() const { return device; }

	bool Open() override;
	void Close() override;
	bool IsOpen() const override;

	void PopFrames(Vector<VideoFrame>& out) override;
	VideoStats GetStats() const override { return stats; }

	bool SetFormat(VideoPixelFormat f, Size s, int frames_per_sec) override;
	bool GetFormat(VideoPixelFormat& f, Size& s, int& frames_per_sec) const override;
};

#endif
