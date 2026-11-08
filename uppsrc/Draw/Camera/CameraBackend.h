#ifndef _Draw_Camera_CameraBackend_h_
#define _Draw_Camera_CameraBackend_h_

class CameraBackend : public VideoBackend {
public:
	virtual ~CameraBackend() = default;
	virtual void PopFrames(Vector<CameraFrame>& out) = 0;
	virtual CameraStats GetCameraStats() const = 0;

	VideoStats GetStats() const override { return GetCameraStats(); }
};

#endif
