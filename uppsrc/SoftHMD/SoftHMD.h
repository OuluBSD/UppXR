#ifndef _SoftHMD_SoftHMD_h_
#define _SoftHMD_SoftHMD_h_

// openhmd commit: dfac0203376552c5274976c42f0757b31310c483

#include <string.h>
#include <wchar.h>
#if __has_include(<hidapi/hidapi.h>)
	#include <hidapi/hidapi.h>
#elif __has_include(<hidapi.h>)
	#include <hidapi.h>
#elif __has_include(<hidapi/hidapi_libusb.h>)
	#include <hidapi/hidapi_libusb.h>
#else
	struct hid_device_;
	typedef struct hid_device_ hid_device;
	struct hid_device_info {
		char *path;
		unsigned short vendor_id;
		unsigned short product_id;
		wchar_t *serial_number;
		unsigned short release_number;
		wchar_t *manufacturer_string;
		wchar_t *product_string;
		unsigned short usage_page;
		unsigned short usage;
		int interface_number;
		struct hid_device_info *next;
	};
	inline int hid_init() { return 0; }
	inline int hid_exit() { return 0; }
	inline struct hid_device_info *hid_enumerate(unsigned short, unsigned short) { return 0; }
	inline void hid_free_enumeration(struct hid_device_info *) {}
	inline hid_device *hid_open(unsigned short, unsigned short, const wchar_t *) { return 0; }
	inline hid_device *hid_open_path(const char *) { return 0; }
	inline int hid_write(hid_device *, const unsigned char *, size_t) { return -1; }
	inline int hid_read_timeout(hid_device *, unsigned char *, size_t, int) { return -1; }
	inline int hid_read(hid_device *, unsigned char *, size_t) { return -1; }
	inline int hid_set_nonblocking(hid_device *, int) { return -1; }
	inline int hid_send_feature_report(hid_device *, const unsigned char *, size_t) { return -1; }
	inline int hid_get_feature_report(hid_device *, unsigned char *, size_t) { return -1; }
	inline void hid_close(hid_device *) {}
#endif
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifdef flagPOSIX
	#include <time.h>
	#include <sys/time.h>
	#include <pthread.h>
	#ifdef __CYGWIN__
		#define CLOCK_MONOTONIC (clockid_t)4
	#endif
	#ifndef _POSIX_C_SOURCE
		#define _POSIX_C_SOURCE 199309L
	#endif
#endif

#ifdef __ANDROID__
	#include <android/sensor.h>
	#include <poll.h>
	#include <pthread.h>
	#include <sched.h>
	#include <android/configuration.h>
	#include <android/looper.h>
	#include <android/native_activity.h>
#endif


#include <Geometry/Geometry.h>
#include <ComputerVision/ComputerVision.h>



#define NAMESPACE_HMD_BEGIN		namespace Upp { namespace HMD {
#define NAMESPACE_HMD_END		}}


#include "Public.h"
#include "Internal.h"
#include "Log.h"
#include "Math.h"
#include "Fusion.h"
#include "Platform.h"
#include "Utils.h"
#include "Shaders.h"
#include "Wmr.h"
#include "Oculus.h"
#include "Android.h"
#include "Camera.h"
#include "Tracking.h"
#include "TrackingImpl.h"
#include "Psvr.h"
#include "ConfigKey.h"
#include "Miniz.h"
#include "NxJson.h"
#include "System.h"
#include "Daemon.h"


#endif
