#ifdef flagLINUX
#include <plugin/libv4l2/libv4l2.h>
#include <plugin/jpg/jpg.h>
#endif

#include "Camera.h"

NAMESPACE_UPP

#ifdef flagLINUX
static Image DecodeYUYV(const byte* src, int w, int h) {
	ImageBuffer ib(w, h);
	const byte* s = src;
	RGBA* t = ib.Begin();
	for (int i = 0; i < w * h / 2; i++) {
		int y0 = s[0]; int u0 = s[1]; int y1 = s[2]; int v0 = s[3];
		s += 4;
		auto YUV2RGB = [](int y, int u, int v, RGBA& p) {
			int c = y - 16; int d = u - 128; int e = v - 128;
			int r = (298 * c + 409 * e + 128) >> 8;
			int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
			int b = (298 * c + 516 * d + 128) >> 8;
			p.b = (byte)clamp(r, 0, 255);
			p.g = (byte)clamp(g, 0, 255);
			p.r = (byte)clamp(b, 0, 255);
			p.a = 255;
		};
		YUV2RGB(y0, u0, v0, *t++);
		YUV2RGB(y1, u0, v0, *t++);
	}
	return ib;
}

static unsigned int ToFourCC(VideoPixelFormat fmt) {
	switch (fmt) {
	case VID_PIX_MJPEG: return V4L2_PIX_FMT_MJPEG;
	case VID_PIX_YUYV: return V4L2_PIX_FMT_YUYV;
	default: return 0;
	}
}
#endif

bool VideoV4L2Backend::Open() {
#ifdef flagLINUX
	if (opened)
		return true;
	if (device.IsEmpty())
		return false;
	unsigned int fourcc = ToFourCC(fmt);
	int req_fps = fps > 0 ? fps : 30;
	V4L2DeviceParameters params(device.Begin(), fourcc, size.cx, size.cy, req_fps, 1);
	V4l2Capture* capture = V4l2Capture::create(params);
	if (!capture || !capture->isReady()) {
		if (capture)
			delete capture;
		return false;
	}
	cap = capture;
	buffer.SetCount(capture->getBufferSize());
	opened = true;
	return true;
#else
	return false;
#endif
}

void VideoV4L2Backend::Close() {
#ifdef flagLINUX
	if (!opened)
		return;
	if (cap) {
		delete static_cast<V4l2Capture*>(cap);
		cap = nullptr;
	}
	buffer.Clear();
	opened = false;
#endif
}

bool VideoV4L2Backend::IsOpen() const {
	return opened;
}

void VideoV4L2Backend::PopFrames(Vector<VideoFrame>& out) {
	out.Clear();
#ifdef flagLINUX
	if (!opened || !cap)
		return;
	V4l2Capture* capture = static_cast<V4l2Capture*>(cap);
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	if (!capture->isReadable(&tv))
		return;
	size_t n = capture->read((char*)buffer.Begin(), capture->getBufferSize());
	if (n == 0)
		return;
	Image img;
	String fcc = V4l2Device::fourcc(capture->getFormat()).c_str();
	if (fcc == "MJPG") {
		JPGRaster raster;
		img = raster.LoadString(String((const char*)buffer.Begin(), (int)n));
		if (!img.IsEmpty()) {
			ImageBuffer ib(img);
			for (RGBA& p : ib) Swap(p.r, p.b);
			img = ib;
		}
	}
	else if (fcc == "YUYV") {
		img = DecodeYUYV(buffer.Begin(), capture->getWidth(), capture->getHeight());
	}
	if (img.IsEmpty())
		return;
	VideoFrame& vf = out.Add();
	vf.img = img;
	vf.size = img.GetSize();
	vf.timestamp_us = usecs();
	vf.format = VID_PIX_RGBA8;
	stats.frame_count++;
	stats.last_timestamp_us = vf.timestamp_us;
#endif
}

bool VideoV4L2Backend::SetFormat(VideoPixelFormat f, Size s, int frames_per_sec) {
	fmt = f;
	size = s;
	fps = frames_per_sec;
	return true;
}

bool VideoV4L2Backend::GetFormat(VideoPixelFormat& f, Size& s, int& frames_per_sec) const {
	f = fmt;
	s = size;
	frames_per_sec = fps;
	return true;
}

END_UPP_NAMESPACE
