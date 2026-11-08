#ifndef _Draw_Camera_CameraTypes_h_
#define _Draw_Camera_CameraTypes_h_

struct CameraFrame : VideoFrame {
	bool is_bright = false;
	int exposure = 0;
	int64 serial = 0;
};

struct CameraStats : VideoStats {
	int bright_frames = 0;
	int dark_frames = 0;
	int last_exposure = 0;
};

#endif
