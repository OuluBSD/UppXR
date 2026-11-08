#ifndef _Draw_Camera_CameraCalibration_h_
#define _Draw_Camera_CameraCalibration_h_

struct UsbDeviceIds {
	int vendor_id = -1;
	int product_id = -1;
};

bool GetUsbIdsForVideoDevice(const String& device_path, UsbDeviceIds& ids);
String FindCalibrationPathForUsb(int vendor_id, int product_id);
bool LoadStereoCalibrationFile(const String& path, StereoCalibrationData& out);
bool LoadStereoCalibrationFromUsbDevice(const String& device_path, StereoCalibrationData& out, String* out_path = nullptr);

#endif
