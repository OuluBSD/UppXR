#ifndef _SoftHMD_OCULUS_H
#define _SoftHMD_OCULUS_H


NAMESPACE_HMD_BEGIN


#define OCULUS_VR_INC_ID 0x2833
#define SAMSUNG_ELECTRONICS_CO_ID 0x04e8
#define RIFT_CV1_PID 0x0031

#define RIFT_FEATURE_BUFFER_SIZE 256

typedef enum {
	RIFT_CMD_SENSOR_CONFIG = 2,
	RIFT_CMD_IMU_CALIBRATION = 3, /* Not used. The HMD does calibration handling */
	RIFT_CMD_RANGE = 4,
	RIFT_CMD_DK1_KEEP_ALIVE = 8,
	RIFT_CMD_DISPLAY_INFO = 9,
	RIFT_CMD_TRACKING_CONFIG = 0xc,
	RIFT_CMD_POSITION_INFO = 0xf,
	RIFT_CMD_PATTERN_INFO = 0x10,
	RIFT_CMD_CV1_KEEP_ALIVE = 0x11,
	RIFT_CMD_RADIO_CONTROL = 0x1a,
	RIFT_CMD_RADIO_READ_DATA = 0x1b,
	RIFT_CMD_ENABLE_COMPONENTS = 0x1d,
} RiftSensorFeatureCmd;

typedef enum {
	RIFT_HMD_RADIO_READ_FLASH_CONTROL =	0x0a
} RiftHmdRadioReadCmd;

typedef enum {
	RIFT_CF_SENSOR,
	RIFT_CF_HMD
} RiftCoordinateFrame;

typedef enum {
	RIFT_IRQ_SENSORS_DK1 = 1,
	RIFT_IRQ_SENSORS_DK2 = 11
} RiftIrqCmd;

typedef enum {
	RIFT_DT_NONE,
	RIFT_DT_SCREEN_ONLY,
	RIFT_DT_DISTORTION
} RiftDistortionType;

typedef enum {
	RIFT_COMPONENT_DISPLAY = 1,
	RIFT_COMPONENT_AUDIO = 2,
	RIFT_COMPONENT_LEDS = 4
} RiftComponentType;

// Sensor config flags
#define RIFT_SCF_RAW_MODE           0x01
#define RIFT_SCF_CALIBRATION_TEST   0x02
#define RIFT_SCF_USE_CALIBRATION    0x04
#define RIFT_SCF_AUTO_CALIBRATION   0x08
#define RIFT_SCF_MOTION_KEEP_ALIVE  0x10
#define RIFT_SCF_COMMAND_KEEP_ALIVE 0x20
#define RIFT_SCF_SENSOR_COORDINATES 0x40

typedef enum {
	RIFT_TRACKING_ENABLE        	= 0x01,
	RIFT_TRACKING_AUTO_INCREMENT	= 0x02,
	RIFT_TRACKING_USE_CARRIER    	= 0x04,
	RIFT_TRACKING_SYNC_INPUT    	= 0x08,
	RIFT_TRACKING_VSYNC_LOCK    	= 0x10,
	RIFT_TRACKING_CUSTOM_PATTERN	= 0x20
} RiftTrackingConfigFlags;

#define RIFT_TRACKING_EXPOSURE_US_DK2           350
#define RIFT_TRACKING_EXPOSURE_US_CV1           399
#define RIFT_TRACKING_PERIOD_US_DK2             16666
#define RIFT_TRACKING_PERIOD_US_CV1             19200
#define RIFT_TRACKING_VSYNC_OFFSET              0
#define RIFT_TRACKING_DUTY_CYCLE                0x7f

typedef struct {
	uint16 command_id;
	uint16 accel_scale;
	uint16 gyro_scale;
	uint16 mag_scale;
} PktSensorRange;

typedef struct {
	int32 accel[3];
	int32 gyro[3];
} PktTrackerSample;

typedef struct {
	uint8 num_samples;
	uint16 total_sample_count;
	int16 temperature;
	uint32 timestamp;
	PktTrackerSample samples[3];
	int16 mag[3];

	uint16 frame_count;        /* HDMI input frame count */
	uint32 frame_timestamp;    /* HDMI vsync timestamp */
	uint8 frame_id;            /* frame id pixel readback */
	uint8 led_pattern_phase;
	uint16 exposure_count;
	uint32 exposure_timestamp;
} PktTrackerSensor;

typedef struct {
    uint16 command_id;
    uint8 flags;
    uint16 packet_interval;
    uint16 keep_alive_interval; // in ms
} PktSensorConfig;

typedef struct {
	uint16 command_id;
	uint8 pattern;
	uint8 flags;
	uint8 reserved;
	uint16 exposure_us;
	uint16 period_us;
	uint16 vsync_offset;
	uint8 duty_cycle;
} PktTrackingConfig;

typedef struct {
	uint16 command_id;
	RiftDistortionType distortion_type;
	uint8 distortion_type_opts;
	uint16 h_resolution, v_resolution;
	float h_screen_size, v_screen_size;
	float v_center;
	float lens_separation;
	float eye_to_screen_distance[2];
	float distortion_k[6];
} PktSensorDisplayInfo;

typedef struct {
	uint16 command_id;
	uint16 keep_alive_interval;
} PktKeepAlive;

typedef struct {
	uint8 flags;
	int32 pos_x;
	int32 pos_y;
	int32 pos_z;
	int16 dir_x;
	int16 dir_y;
	int16_t dir_z;
	uint8 index;
	uint8 num;
	uint8 type;
} PktPositionInfo;

typedef struct {
	uint8 pattern_length;
	uint32 pattern;
	uint16 index;
	uint16 num;
} PktLedPatternReport;

typedef struct {
	// Relative position in micrometers
	vec3 pos;
	// Normal
	vec3 dir;
	// Blink pattern
	uint16 pattern;
} RiftLed;

typedef struct {
	vec3 imu_position;
	float gyro_calibration[12];
	float acc_calibration[12];
	uint16 joy_x_range_min;
	uint16 joy_x_range_max;
	uint16 joy_x_dead_min;
	uint16 joy_x_dead_max;
	uint16 joy_y_range_min;
	uint16 joy_y_range_max;
	uint16 joy_y_dead_min;
	uint16 joy_y_dead_max;
	uint16 trigger_min_range;
	uint16 trigger_mid_range;
	uint16 trigger_max_range;
	uint16 middle_min_range;
	uint16 middle_mid_range;
	uint16 middle_max_range;
	bool middle_flipped;
	uint16 cap_sense_min[8];
	uint16 cap_sense_touch[8];
} RiftTouchCalibration;

#define RIFT_RADIO_REPORT_ID			0x0c
#define RIFT_RADIO_REPORT_SIZE			64

#define RIFT_REMOTE				1
#define RIFT_TOUCH_CONTROLLER_LEFT		2
#define RIFT_TOUCH_CONTROLLER_RIGHT		3

#define RIFT_REMOTE_BUTTON_UP			0x001
#define RIFT_REMOTE_BUTTON_DOWN			0x002
#define RIFT_REMOTE_BUTTON_LEFT			0x004
#define RIFT_REMOTE_BUTTON_RIGHT		0x008
#define RIFT_REMOTE_BUTTON_OK			0x010
#define RIFT_REMOTE_BUTTON_PLUS			0x020
#define RIFT_REMOTE_BUTTON_MINUS		0x040
#define RIFT_REMOTE_BUTTON_OCULUS		0x080
#define RIFT_REMOTE_BUTTON_BACK			0x100

typedef struct {
	uint16 buttons;
} PktRiftRemoteMessage;

#define RIFT_TOUCH_CONTROLLER_BUTTON_A		0x01
#define RIFT_TOUCH_CONTROLLER_BUTTON_X		0x01
#define RIFT_TOUCH_CONTROLLER_BUTTON_B		0x02
#define RIFT_TOUCH_CONTROLLER_BUTTON_Y		0x02
#define RIFT_TOUCH_CONTROLLER_BUTTON_MENU	0x04
#define RIFT_TOUCH_CONTROLLER_BUTTON_OCULUS	0x04
#define RIFT_TOUCH_CONTROLLER_BUTTON_STICK	0x08

#define RIFT_TOUCH_CONTROLLER_ADC_STICK		0x01
#define RIFT_TOUCH_CONTROLLER_ADC_B_Y		0x02
#define RIFT_TOUCH_CONTROLLER_ADC_TRIGGER	0x03
#define RIFT_TOUCH_CONTROLLER_ADC_A_X		0x04
#define RIFT_TOUCH_CONTROLLER_ADC_REST		0x08

#define RIFT_TOUCH_CONTROLLER_HAPTIC_COUNTER	0x23

typedef struct {
	uint32 timestamp;
	int16 accel[3];
	int16 gyro[3];
	uint8 buttons;
	uint16 trigger;
	uint16 grip;
	uint16 stick[2];
	uint8 adc_channel;
	uint16 adc_value;
} PktRiftTouchMessage;

typedef struct {
	bool valid;
	uint16 flags;
	uint8 device_type;
	union {
		PktRiftRemoteMessage remote;
		PktRiftTouchMessage touch;
	};
} PktRiftRadioMessage;

typedef struct {
	uint8 id;
	PktRiftRadioMessage message[2];
} PktRiftRadioReport;

Driver* CreateOculusDriver(Context* ctx);


NAMESPACE_HMD_END


#endif
