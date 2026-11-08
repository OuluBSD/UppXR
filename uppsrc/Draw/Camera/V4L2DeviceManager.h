#ifndef _Draw_Camera_V4L2DeviceManager_h_
#define _Draw_Camera_V4L2DeviceManager_h_

struct VideoDeviceFormatResolution : Moveable<VideoDeviceFormatResolution> {
	Size size;
	double fps = 0.0;
};

struct VideoDeviceFormat : Moveable<VideoDeviceFormat> {
	String description;
	VideoPixelFormat format = VID_PIX_UNKNOWN;
	uint32 pixelformat = 0;
	Vector<VideoDeviceFormatResolution> resolutions;
};

struct VideoDeviceCaps : Moveable<VideoDeviceCaps> {
	Vector<VideoDeviceFormat> formats;
};

class V4L2DeviceManager : public VideoDeviceManager {
	bool include_formats = false;

#ifdef flagLINUX
	bool ReadFormats(int fd, VideoDeviceCaps& caps);
	VideoPixelFormat MapFormat(dword pixfmt) const;
	void AddResolution(VideoDeviceFormat& fmt, int w, int h, double fps);
#endif

public:
	void SetIncludeFormats(bool b) { include_formats = b; }
	bool GetIncludeFormats() const { return include_formats; }
	void Enumerate(Vector<VideoDeviceInfo>& out) override;
	bool EnumerateCaps(const String& path, VideoDeviceCaps& caps);
};

#endif
