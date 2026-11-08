#ifndef _Draw_Video_VideoBackend_h_
#define _Draw_Video_VideoBackend_h_

class VideoBackend {
public:
	virtual ~VideoBackend() = default;

	virtual bool Open() = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;

	virtual void PopFrames(Vector<VideoFrame>& out) = 0;
	virtual VideoStats GetStats() const = 0;

	virtual bool SetFormat(VideoPixelFormat fmt, Size size, int fps) { return false; }
	virtual bool GetFormat(VideoPixelFormat& fmt, Size& size, int& fps) const { return false; }
};

#endif
