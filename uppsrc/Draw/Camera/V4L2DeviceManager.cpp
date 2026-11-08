#ifdef flagLINUX
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "Camera.h"

NAMESPACE_UPP

#ifdef flagLINUX
VideoPixelFormat V4L2DeviceManager::MapFormat(dword pixfmt) const {
	switch (pixfmt) {
	case V4L2_PIX_FMT_MJPEG: return VID_PIX_MJPEG;
	case V4L2_PIX_FMT_YUYV: return VID_PIX_YUYV;
	case V4L2_PIX_FMT_RGB24: return VID_PIX_RGB8;
	case V4L2_PIX_FMT_BGR24: return VID_PIX_BGR8;
	default: return VID_PIX_UNKNOWN;
	}
}

void V4L2DeviceManager::AddResolution(VideoDeviceFormat& fmt, int w, int h, double fps) {
	VideoDeviceFormatResolution res;
	res.size = Size(w, h);
	res.fps = fps;
	fmt.resolutions.Add(res);
}

bool V4L2DeviceManager::ReadFormats(int fd, VideoDeviceCaps& caps) {
	struct v4l2_fmtdesc fmtdesc;
	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
		VideoDeviceFormat& fmt = caps.formats.Add();
		fmt.description = (const char*)fmtdesc.description;
		fmt.format = MapFormat(fmtdesc.pixelformat);
		fmt.pixelformat = fmtdesc.pixelformat;

		struct v4l2_frmsizeenum frmsize;
		memset(&frmsize, 0, sizeof(frmsize));
		frmsize.pixel_format = fmtdesc.pixelformat;
		frmsize.index = 0;
		while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
			struct v4l2_frmivalenum ival;
			memset(&ival, 0, sizeof(ival));
			ival.index = 0;
			ival.pixel_format = frmsize.pixel_format;
			ival.width = frmsize.discrete.width;
			ival.height = frmsize.discrete.height;
			while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) >= 0) {
				double frame_time = (double)ival.discrete.numerator / (double)ival.discrete.denominator;
				double fps = frame_time > 0 ? 1.0 / frame_time : 0.0;
				if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
					AddResolution(fmt, frmsize.discrete.width, frmsize.discrete.height, fps);
				else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
					AddResolution(fmt, frmsize.stepwise.max_width, frmsize.stepwise.max_height, fps);
				ival.index++;
			}
			frmsize.index++;
		}
		fmtdesc.index++;
	}
	return caps.formats.GetCount() > 0;
}
#endif

void V4L2DeviceManager::Enumerate(Vector<VideoDeviceInfo>& out) {
	out.Clear();
#ifdef flagLINUX
	for (int i = 0; i < 64; i++) {
		String path = "/dev/video" + AsString(i);
		int fd = open(path, O_RDWR);
		if (fd < 0)
			continue;
		struct v4l2_capability cap;
		if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
			if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
				VideoDeviceInfo& info = out.Add();
				info.id = AsString(i);
				info.name = (const char*)cap.card;
				info.path = path;
				info.driver = (const char*)cap.driver;
				info.bus_info = (const char*)cap.bus_info;
				info.capabilities = cap.capabilities;
			}
		}
		close(fd);
	}
#endif
}

bool V4L2DeviceManager::EnumerateCaps(const String& path, VideoDeviceCaps& caps) {
	caps.formats.Clear();
#ifdef flagLINUX
	int fd = open(path.Begin(), O_RDWR);
	if (fd < 0)
		return false;
	bool ok = ReadFormats(fd, caps);
	close(fd);
	return ok;
#else
	return false;
#endif
}

END_UPP_NAMESPACE
