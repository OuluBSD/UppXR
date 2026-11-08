#ifndef _Draw_Video_VideoTypes_h_
#define _Draw_Video_VideoTypes_h_

enum VideoPixelFormat {
	VID_PIX_UNKNOWN = 0,
	VID_PIX_RGBA8,
	VID_PIX_BGRA8,
	VID_PIX_RGB8,
	VID_PIX_BGR8,
	VID_PIX_LUMA8,
	VID_PIX_YUYV,
	VID_PIX_MJPEG
};

struct VideoPlane {
	const byte* data = nullptr;
	int stride_bytes = 0;
	int bytes = 0;
};

struct VideoFrame : Moveable<VideoFrame> {
	Image img;
	Size size;
	int64 timestamp_us = 0;
	dword flags = 0;
	VideoPixelFormat format = VID_PIX_UNKNOWN;
	int plane_count = 0;
	VideoPlane planes[3];
};

struct VideoStats {
	int frame_count = 0;
	int dropped_frames = 0;
	int64 last_timestamp_us = 0;
};

#endif
