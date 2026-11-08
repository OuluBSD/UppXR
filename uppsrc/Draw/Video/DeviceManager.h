#ifndef _Draw_Video_DeviceManager_h_
#define _Draw_Video_DeviceManager_h_

struct VideoDeviceInfo : Moveable<VideoDeviceInfo> {
	String id;
	String name;
	String path;
	String driver;
	String bus_info;
	uint32 capabilities = 0;
};

class VideoDeviceManager {
public:
	virtual ~VideoDeviceManager() = default;
	virtual void Enumerate(Vector<VideoDeviceInfo>& out) = 0;
};

#endif
