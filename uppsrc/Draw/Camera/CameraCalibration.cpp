#ifdef flagLINUX
#include <unistd.h>
#include <limits.h>
#endif

#include "Camera.h"

NAMESPACE_UPP

namespace {

String GetVideoSysPath(const String& device_path) {
	String base = GetFileName(device_path);
	if (base.IsEmpty())
		return String();
	return AppendFileName(AppendFileName("/sys/class/video4linux", base), "device");
}

bool ReadHexFile(const String& path, int& out) {
	if (!FileExists(path))
		return false;
	String s = TrimBoth(LoadFile(path));
	if (s.IsEmpty())
		return false;
	if (s.StartsWith("0x") || s.StartsWith("0X"))
		s = s.Mid(2);
	out = ScanInt(~s, nullptr, 16);
	return true;
}

bool ReadUsbIdsFromPath(const String& start_path, UsbDeviceIds& ids) {
#ifdef flagLINUX
	String path = start_path;
	for (int i = 0; i < 6 && !path.IsEmpty() && path != "/"; i++) {
		int vid = -1, pid = -1;
		if (ReadHexFile(AppendFileName(path, "idVendor"), vid) &&
		    ReadHexFile(AppendFileName(path, "idProduct"), pid)) {
			ids.vendor_id = vid;
			ids.product_id = pid;
			return true;
		}
		path = GetFileFolder(path);
	}
#endif
	return false;
}

int ParseIdValue(const Value& v) {
	if (IsNull(v))
		return -1;
	if (v.Is<int>())
		return (int)v;
	if (v.Is<String>()) {
		String s = (String)v;
		s = TrimBoth(s);
		if (s.StartsWith("0x") || s.StartsWith("0X"))
			s = s.Mid(2);
		return ScanInt(~s, nullptr, 16);
	}
	return -1;
}

}

bool GetUsbIdsForVideoDevice(const String& device_path, UsbDeviceIds& ids) {
	ids.vendor_id = -1;
	ids.product_id = -1;
#ifdef flagLINUX
	String sys_path = GetVideoSysPath(device_path);
	if (sys_path.IsEmpty())
		return false;
	return ReadUsbIdsFromPath(sys_path, ids);
#else
	return false;
#endif
}

String FindCalibrationPathForUsb(int vendor_id, int product_id) {
	if (vendor_id < 0 || product_id < 0)
		return String();
	String list_path = "share/calibration/list.json";
	if (FileExists(list_path)) {
		Value json = ParseJSON(LoadFile(list_path));
		if (json.Is<ValueArray>()) {
			const ValueArray& arr = json.Get<ValueArray>();
			for (int i = 0; i < arr.GetCount(); i++) {
				const Value& entry = arr[i];
				if (!entry.Is<ValueMap>())
					continue;
				ValueMap m = entry;
				int vid = ParseIdValue(m.Get("vendor_id", Value()));
				int pid = ParseIdValue(m.Get("product_id", Value()));
				if (vid == vendor_id && pid == product_id) {
					String path = m.Get("path", Value());
					if (!path.IsEmpty() && FileExists(path))
						return path;
				}
			}
		}
	}
	String fallback = Format("share/calibration/%04x_%04x/calibration.stcal", vendor_id, product_id);
	if (FileExists(fallback))
		return fallback;
	return String();
}

bool LoadStereoCalibrationFile(const String& path, StereoCalibrationData& out) {
	if (path.IsEmpty())
		return false;
	return VisitFromJsonFile(out, path);
}

bool LoadStereoCalibrationFromUsbDevice(const String& device_path, StereoCalibrationData& out, String* out_path) {
	UsbDeviceIds ids;
	if (!GetUsbIdsForVideoDevice(device_path, ids))
		return false;
	String path = FindCalibrationPathForUsb(ids.vendor_id, ids.product_id);
	if (path.IsEmpty())
		return false;
	if (!LoadStereoCalibrationFile(path, out))
		return false;
	if (out_path)
		*out_path = path;
	return true;
}

END_UPP_NAMESPACE
