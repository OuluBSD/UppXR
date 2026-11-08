#include "SoftHMD.h"
#include "Oculus.h"


NAMESPACE_HMD_BEGIN


#define OHMD_GRAVITY_EARTH 9.80665 // m/s²

#define UDEV_WIKI_URL "https://github.com/OpenHMD/OpenHMD/wiki/Udev-rules-list"

#define TICK_LEN (1.0f / 1000.0f) // 1000 Hz ticks
#define KEEP_ALIVE_VALUE (10 * 1000)
#define SETFLAG(_s, _flag, _val) (_s) = ((_s) & ~(_flag)) | ((_val) ? (_flag) : 0)

typedef enum {
	REV_DK1,
	REV_DK2,
	REV_CV1,

	REV_GEARVR_GEN1
} RiftRevision;

typedef struct {
	const char* name;
	int company;
	int id;
	int iface;
	RiftRevision rev;
} RiftDevices;

struct RiftHmd;

typedef struct {
	Device base;
	int id;
	bool opened;

	struct RiftHmd *hmd;
} RiftDevicePriv;

typedef struct {
	RiftDevicePriv base;

	int device_num;
	Fusion imu_fusion;

	bool have_calibration;
	RiftTouchCalibration calibration;

	bool time_valid;
	uint32 last_timestamp;

	uint8 buttons;

	float trigger;
	float grip;
	float stick[2];
	float cap_a_x;
	float cap_b_y;
	float cap_rest;
	float cap_stick;
	float cap_trigger;
	uint8 haptic_counter;
} RiftTouchController;

struct RiftHmd {
	Context* ctx;
	int use_count;

	::hid_device* handle;
	::hid_device* radio_handle;
	PktSensorRange sensor_range;
	PktSensorDisplayInfo display_info;
	RiftCoordinateFrame coordinate_frame, hw_coordinate_frame;
	PktSensorConfig sensor_config;
	PktTrackerSensor sensor;
	uint32 last_imu_timestamp;
	double last_keep_alive;
	Fusion sensor_fusion;
	vec3 raw_mag, raw_accel, raw_gyro;

	struct {
		vec3 pos;
	} imu;

	uint8 radio_address[5];
	RiftLed *leds;
	uint8 num_leds;

	uint16 remote_buttons_state;

	/* OpenHMD output devices */
	RiftDevicePriv hmd_dev;
	RiftTouchController touch_dev[2];
};

typedef struct RiftDeviceList {
	char path[HMD_STR_SIZE];
	struct RiftHmd *hmd;

	struct RiftDeviceList* next;
} RiftDeviceList;

/* Global list of (probably 1) active HMD devices */
static RiftDeviceList* rift_hmds;

// Forward declarations of packet decoding functions
static bool decode_position_info(PktPositionInfo* p, const unsigned char* buffer, int size);
static bool decode_led_pattern_info(PktLedPatternReport * p, const unsigned char* buffer, int size);
static bool decode_sensor_range(PktSensorRange* range, const unsigned char* buffer, int size);
static bool decode_sensor_display_info(PktSensorDisplayInfo* info, const unsigned char* buffer, int size);
static bool decode_sensor_config(PktSensorConfig* config, const unsigned char* buffer, int size);
static bool decode_tracker_sensor_msg_dk1(PktTrackerSensor* msg, const unsigned char* buffer, int size);
static bool decode_tracker_sensor_msg_dk2(PktTrackerSensor* msg, const unsigned char* buffer, int size);
static bool decode_radio_address(uint8 radio_address[5], const unsigned char* buffer, int size);
static bool decode_rift_radio_report(PktRiftRadioReport *r, const unsigned char* buffer, int size);
static int encode_tracking_config(unsigned char* buffer, const PktTrackingConfig* tracking);
static int encode_sensor_config(unsigned char* buffer, const PktSensorConfig* config);
static int encode_dk1_keep_alive(unsigned char* buffer, const PktKeepAlive* keep_alive);
static int encode_enable_components(unsigned char* buffer, bool display, bool audio, bool leds);
static int encode_radio_control_cmd(unsigned char* buffer, uint8 a, uint8 b, uint8 c);
static int encode_radio_data_read_cmd(unsigned char *buffer, uint16 offset, uint16 length);
static void dump_packet_sensor_range(const PktSensorRange* range);
static void dump_packet_sensor_config(const PktSensorConfig* config);
static void dump_packet_sensor_display_info(const PktSensorDisplayInfo* info);
static void dump_packet_tracker_sensor(const PktTrackerSensor* sensor);

// Radio functions
static int rift_touch_get_calibration(::hid_device *handle, int device_id, RiftTouchCalibration *calibration);
static bool rift_hmd_radio_get_address(::hid_device *handle, uint8 address[5]);


#define SKIP8 (buffer++)
#define SKIP16 (buffer+=2)
#define SKIP_CMD (buffer++)
#define READ8 *(buffer++);
#define READ16 (*buffer | (*(buffer + 1) << 8)); buffer += 2;
#define READ32 (*buffer | (*(buffer + 1) << 8) | (*(buffer + 2) << 16) | (*(buffer + 3) << 24)); buffer += 4;
#define READFLOAT (*((float*)buffer)); buffer += 4;
#define READFIXED ((float)(*buffer | (*(buffer + 1) << 8) | (*(buffer + 2) << 16) | (*(buffer + 3) << 24)) / 1000000.0f); buffer += 4;

#define WRITE8(_val) *(buffer++) = (_val);
#define WRITE16(_val) WRITE8((_val) & 0xff); WRITE8(((_val) >> 8) & 0xff);
#define WRITE32(_val) WRITE16((_val) & 0xffff); WRITE16(((_val) >> 16) & 0xffff);

static bool decode_position_info(PktPositionInfo* p, const unsigned char* buffer, int size)
{
	if(size != 30) {
		LOGE("invalid packet size (expected 30 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	SKIP16;
	p->flags = READ8;
	p->pos_x = READ32;
	p->pos_y = READ32;
	p->pos_z = READ32;
	p->dir_x = READ16;
	p->dir_y = READ16;
	p->dir_z = READ16;
	SKIP16;
	p->index = READ8;
	SKIP8;
	p->num = READ8;
	SKIP8;
	p->type = READ8;

	return true;
}

static bool decode_led_pattern_info(PktLedPatternReport * p, const unsigned char* buffer, int size)
{
	if(size != 12) {
		LOGE("invalid packet size (expected 12 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	SKIP16;
	p->pattern_length = READ8;
	p->pattern = READ32;
	p->index = READ16;
	p->num = READ16;

	return true;
}

static bool decode_sensor_range(PktSensorRange* range, const unsigned char* buffer, int size)
{
	if(!(size == 8 || size == 9)){
		LOGE("invalid packet size (expected 8 or 9 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	range->command_id = READ16;
	range->accel_scale = READ8;
	range->gyro_scale = READ16;
	range->mag_scale = READ16;

	return true;
}

static bool decode_sensor_display_info(PktSensorDisplayInfo* info, const unsigned char* buffer, int size)
{
	if(!(size == 56 || size == 57)){
		LOGE("invalid packet size (expected 56 or 57 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	info->command_id = READ16;
	info->distortion_type = (RiftDistortionType)READ8;
	info->h_resolution = READ16;
	info->v_resolution = READ16;
	info->h_screen_size = READFIXED;
	info->v_screen_size = READFIXED;
	info->v_center = READFIXED;
	info->lens_separation = READFIXED;
	info->eye_to_screen_distance[0] = READFIXED;
	info->eye_to_screen_distance[1] = READFIXED;

	info->distortion_type_opts = 0;

	for(int i = 0; i < 6; i++){
		info->distortion_k[i] = READFLOAT;
	}

	return true;
}

static bool decode_sensor_config(PktSensorConfig* config, const unsigned char* buffer, int size)
{
	if(!(size == 7 || size == 8)){
		LOGE("invalid packet size (expected 7 or 8 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	config->command_id = READ16;
	config->flags = READ8;
	config->packet_interval = READ8;
	config->keep_alive_interval = READ16;

	return true;
}

static void decode_sample(const unsigned char* buffer, int32* smp)
{
	int x = (buffer[0] << 24)          | (buffer[1] << 16) | ((buffer[2] & 0xF8) << 8);
	int y = ((buffer[2] & 0x07) << 29) | (buffer[3] << 21) | (buffer[4] << 13) | ((buffer[5] & 0xC0) << 5);
	int z = ((buffer[5] & 0x3F) << 26) | (buffer[6] << 18) | (buffer[7] << 10);

	smp[0] = x >> 11;
	smp[1] = y >> 11;
	smp[2] = z >> 11;
}

static bool decode_tracker_sensor_msg_dk1(PktTrackerSensor* msg, const unsigned char* buffer, int size)
{
	if(!(size == 62 || size == 64)){
		LOGE("invalid packet size (expected 62 or 64 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	msg->num_samples = READ8;
	msg->total_sample_count = 0;
	msg->timestamp = READ16;
	msg->timestamp *= 1000;
	buffer += 2;
	msg->temperature = READ16;

	msg->num_samples = HMD_MIN(msg->num_samples, 3);
	for(int i = 0; i < msg->num_samples; i++){
		decode_sample(buffer, msg->samples[i].accel);
		buffer += 8;

		decode_sample(buffer, msg->samples[i].gyro);
		buffer += 8;
	}

	buffer += (3 - msg->num_samples) * 16;
	for(int i = 0; i < 3; i++){
		msg->mag[i] = READ16;
	}

	msg->frame_count = 0;
	msg->frame_timestamp = 0;
	msg->frame_id = 0;
	msg->led_pattern_phase = 0;
	msg->exposure_count = 0;
	msg->exposure_timestamp = 0;

	return true;
}

static bool decode_tracker_sensor_msg_dk2(PktTrackerSensor* msg, const unsigned char* buffer, int size)
{
	if(!(size == 64)){
		LOGE("invalid packet size (expected 64 but got %d)", size);
		return false;
	}

	SKIP_CMD;
	SKIP16;
	msg->num_samples = READ8;
	msg->total_sample_count = READ16;
	msg->temperature = READ16;
	msg->timestamp = READ32;

	msg->num_samples = HMD_MIN(msg->num_samples, 2);
	for(int i = 0; i < msg->num_samples; i++){
		decode_sample(buffer, msg->samples[i].accel);
		buffer += 8;

		decode_sample(buffer, msg->samples[i].gyro);
		buffer += 8;
	}

	buffer += (2 - msg->num_samples) * 16;

	for(int i = 0; i < 3; i++){
		msg->mag[i] = READ16;
	}

	msg->frame_count = READ16;
	msg->frame_timestamp = READ32;
	msg->frame_id = READ8;
	msg->led_pattern_phase = READ8;
	msg->exposure_count = READ16;
	msg->exposure_timestamp = READ32;

	return true;
}

static bool decode_radio_address(uint8 radio_address[5], const unsigned char* buffer, int size)
{
	if (size < 8)
		return false;

	SKIP_CMD;
	SKIP16;

	memcpy (radio_address, buffer, 5);

	return true;
}

static bool decode_rift_radio_message(PktRiftRadioMessage *m, const unsigned char* buffer)
{
	int i;

	m->flags = READ16;
	m->device_type = READ8;

	m->valid = (m->flags == 0x1c || m->flags == 0x05);

	if (!m->valid) {
		LOGV("Invalid radio report from unknown remote device type 0x%02x flags 0x%04x",
				m->device_type, m->flags);
		return true;
	}

	switch (m->device_type) {
		case RIFT_REMOTE:
			m->remote.buttons = READ16;
			break;
		case RIFT_TOUCH_CONTROLLER_LEFT:
		case RIFT_TOUCH_CONTROLLER_RIGHT: {
			uint8 tgs[5];

			m->touch.timestamp = READ32;
			for (i = 0; i < 3; i++) {
				m->touch.accel[i] = READ16;
			}
			for (i = 0; i < 3; i++) {
				m->touch.gyro[i] = READ16;
			}

			m->touch.buttons = READ8;

			for (i = 0; i < 5; i++) {
				tgs[i] = READ8;
			}

			m->touch.trigger = tgs[0] | ((tgs[1] & 0x03) << 8);
			m->touch.grip = ((tgs[1] & 0xfc) >> 2) | ((tgs[2] & 0x0f) << 6);
			m->touch.stick[0] = ((tgs[2] & 0xf0) >> 4) | ((tgs[3] & 0x3f) << 4);
			m->touch.stick[1] = ((tgs[3] & 0xc0) >> 6) | ((tgs[4] & 0xff) << 2);

			m->touch.adc_channel = READ8;
			m->touch.adc_value = READ16;
			break;
		}
		default:
			LOGE("Radio report from unknown remote device type 0x%02x flags 0x%04x",
					m->device_type, m->flags);
			return false;
	}

	return true;
}

static bool decode_rift_radio_report(PktRiftRadioReport *r, const unsigned char* buffer, int size)
{
	if (size != RIFT_RADIO_REPORT_SIZE) {
		LOGE("invalid packet size (expected 64 but got %d)", size);
		return false;
	}

	if (buffer[0] != RIFT_RADIO_REPORT_ID) {
		LOGE("Unknown radio report id 0x%02x\n", buffer[0]);
		return false;
	}

	r->id = READ8;
	SKIP16;

	for (int i = 0; i < 2; i++) {
		if (!decode_rift_radio_message (&r->message[i], buffer))
			return false;
		buffer += 28;
	}

	return true;
}

static int encode_sensor_config(unsigned char* buffer, const PktSensorConfig* config)
{
	WRITE8(RIFT_CMD_SENSOR_CONFIG);
	WRITE16(config->command_id);
	WRITE8(config->flags);
	WRITE8(config->packet_interval);
	WRITE16(config->keep_alive_interval);
	return 7;
}

static int encode_tracking_config(unsigned char* buffer, const PktTrackingConfig* tracking)
{
	WRITE8(RIFT_CMD_TRACKING_CONFIG);
	WRITE16(tracking->command_id);
	WRITE8(tracking->pattern);
	WRITE8(tracking->flags);
	WRITE8(tracking->reserved);
	WRITE16(tracking->exposure_us);
	WRITE16(tracking->period_us);
	WRITE16(tracking->vsync_offset);
	WRITE8(tracking->duty_cycle);

	return 13;
}

static int encode_dk1_keep_alive(unsigned char* buffer, const PktKeepAlive* keep_alive)
{
	WRITE8(RIFT_CMD_DK1_KEEP_ALIVE);
	WRITE16(keep_alive->command_id);
	WRITE16(keep_alive->keep_alive_interval);
	return 5;
}

static int encode_enable_components(unsigned char* buffer, bool display, bool audio, bool leds)
{
	uint8 flags = 0;

	WRITE8(RIFT_CMD_ENABLE_COMPONENTS);
	WRITE16(0);

	if (display)
		flags |= RIFT_COMPONENT_DISPLAY;
	if (audio)
		flags |= RIFT_COMPONENT_AUDIO;
	if (leds)
		flags |= RIFT_COMPONENT_LEDS;
	WRITE8(flags);
	return 4;
}

static int encode_radio_control_cmd(unsigned char* buffer, uint8 a, uint8 b, uint8 c)
{
	WRITE8(RIFT_CMD_RADIO_CONTROL);
	WRITE16(0);
	WRITE8(a);
	WRITE8(b);
	WRITE8(c);

	return 6;
}

static int encode_radio_data_read_cmd(unsigned char *buffer, uint16 offset, uint16 length)
{
	int i;
	WRITE8(RIFT_CMD_RADIO_READ_DATA);
	WRITE16(0);
	WRITE16(offset);
	WRITE16(length);

	for (i = 0; i < 28; i++) {
		WRITE8(0);
	}

	return 31;
}

static void dump_packet_sensor_range(const PktSensorRange* range)
{
	LOGD("sensor range\n");
	LOGD("  command id:  %d", range->command_id);
	LOGD("  accel scale: %d", range->accel_scale);
	LOGD("  gyro scale:  %d", range->gyro_scale);
	LOGD("  mag scale:   %d", range->mag_scale);
}

static void dump_packet_sensor_display_info(const PktSensorDisplayInfo* info)
{
	LOGD("display info");
	LOGD("  command id:             %d", info->command_id);
	LOGD("  distortion_type:        %d", info->distortion_type);
	LOGD("  resolution:             %d x %d", info->h_resolution, info->v_resolution);
	LOGD("  screen size:            %f x %f", info->h_screen_size, info->v_screen_size);
	LOGD("  vertical center:        %f", info->v_center);
	LOGD("  lens_separation:        %f", info->lens_separation);
	LOGD("  eye_to_screen_distance: %f, %f", info->eye_to_screen_distance[0], info->eye_to_screen_distance[1]);
	LOGD("  distortion_k:           %f, %f, %f, %f, %f, %f",
		info->distortion_k[0], info->distortion_k[1], info->distortion_k[2],
		info->distortion_k[3], info->distortion_k[4], info->distortion_k[5]);
}

static void dump_packet_sensor_config(const PktSensorConfig* config)
{
	LOGD("sensor config");
	LOGD("  command id:          %u", config->command_id);
	LOGD("  flags:               %02x", config->flags);
	LOGD("    raw mode:                  %d", !!(config->flags & RIFT_SCF_RAW_MODE));
	LOGD("    calibration test:          %d", !!(config->flags & RIFT_SCF_CALIBRATION_TEST));
	LOGD("    use calibration:           %d", !!(config->flags & RIFT_SCF_USE_CALIBRATION));
	LOGD("    auto calibration:          %d", !!(config->flags & RIFT_SCF_AUTO_CALIBRATION));
	LOGD("    motion keep alive:         %d", !!(config->flags & RIFT_SCF_MOTION_KEEP_ALIVE));
	LOGD("    motion command keep alive: %d", !!(config->flags & RIFT_SCF_COMMAND_KEEP_ALIVE));
	LOGD("    sensor coordinates:        %d", !!(config->flags & RIFT_SCF_SENSOR_COORDINATES));
	LOGD("  packet interval:     %u", config->packet_interval);
	LOGD("  keep alive interval: %u", config->keep_alive_interval);
}

static void dump_packet_tracker_sensor(const PktTrackerSensor* sensor)
{
	LOGD("tracker sensor:");
	LOGD("  total sample count: %u", sensor->total_sample_count);
	LOGD("  timestamp:       %u", sensor->timestamp);
	LOGD("  temperature:     %d", sensor->temperature);
	LOGD("  num samples:     %u", sensor->num_samples);
	LOGD("  magnetic field:  %i %i %i", sensor->mag[0], sensor->mag[1], sensor->mag[2]);

	for(int i = 0; i < sensor->num_samples; i++){
		LOGD("    accel: %d %d %d", sensor->samples[i].accel[0], sensor->samples[i].accel[1], sensor->samples[i].accel[2]);
		LOGD("    gyro:  %d %d %d", sensor->samples[i].gyro[0], sensor->samples[i].gyro[1], sensor->samples[i].gyro[2]);
	}
}

static int get_feature_report(RiftHmd *hmd, RiftSensorFeatureCmd cmd, unsigned char* buf)
{
	memset(buf, 0, RIFT_FEATURE_BUFFER_SIZE);
	buf[0] = (unsigned char)cmd;
	return hid_get_feature_report(hmd->handle, buf, RIFT_FEATURE_BUFFER_SIZE);
}

static int send_feature_report(RiftHmd *hmd, unsigned char* buf, int length)
{
	return hid_send_feature_report(hmd->handle, buf, length);
}

static bool rift_hmd_radio_send_cmd(::hid_device *handle, uint8 a, uint8 b, uint8 c)
{
	unsigned char buffer[RIFT_FEATURE_BUFFER_SIZE];
	int cmd_size = encode_radio_control_cmd(buffer, a, b, c);
	int ret_size;

	if (hid_send_feature_report(handle, buffer, cmd_size) < 0)
		return false;

	do {
		memset(buffer, 0, RIFT_FEATURE_BUFFER_SIZE);
		buffer[0] = (unsigned char)RIFT_CMD_RADIO_CONTROL;
		ret_size = hid_get_feature_report(handle, buffer, RIFT_FEATURE_BUFFER_SIZE);
		if (ret_size < 1) {
			LOGE("HMD radio command 0x%02x/%02x/%02x failed - response too small", a, b, c);
			return false;
		}
	} while (buffer[3] & 0x80);

	if (buffer[3] & 0x08)
		return false;

	return true;
}

static int rift_radio_read_flash(::hid_device *handle, uint8 device_type,
				uint16 offset, uint16 length, uint8 *flash_data)
{
	int ret;
	unsigned char buffer[RIFT_FEATURE_BUFFER_SIZE];
	int cmd_size = encode_radio_data_read_cmd(buffer, offset, length);

	ret = hid_send_feature_report(handle, buffer, cmd_size);
	if (ret < 0)
		return ret;

	if (!rift_hmd_radio_send_cmd (handle, 0x03, RIFT_HMD_RADIO_READ_FLASH_CONTROL,
					device_type)) {
		return -1;
	}

	memset(buffer, 0, RIFT_FEATURE_BUFFER_SIZE);
	buffer[0] = (unsigned char)RIFT_CMD_RADIO_READ_DATA;
	ret = hid_get_feature_report(handle, buffer, RIFT_FEATURE_BUFFER_SIZE);
	if (ret < 0)
		return ret;

	memcpy (flash_data, buffer+7, length);

	return ret;
}

static int rift_radio_read_calibration_hash(::hid_device *handle, uint8 device_type,
					    uint8 hash[16])
{
	return rift_radio_read_flash(handle, device_type, 0x1bf0, 16, hash);
}

static int rift_radio_read_calibration(::hid_device *handle, uint8 device_type,
		char **json_out, uint16 *length)
{
	char *json;
	int ret;
	uint8 flash_data[20];
	uint16 json_length;
	uint16 offset;

	ret = rift_radio_read_flash(handle, device_type, 0, 20, flash_data);
	if (ret < 0)
		return ret;

	if (flash_data[0] != 1 || flash_data[1] != 0)
		return -1;
	json_length = (flash_data[3] << 8) | flash_data[2];

	json = (char*)calloc(1, json_length + 1);
	memcpy(json, flash_data + 4, 16);

	for (offset = 20; offset < json_length + 4; offset += 20) {
		uint16 json_offset = offset - 4;

		ret = rift_radio_read_flash(handle, device_type, offset, 20, flash_data);
		if (ret < 0) {
			free(json);
			return ret;
		}

		memcpy(json + json_offset, flash_data, HMD_MIN(20, (int)json_length - (int)json_offset));
	}

	*json_out = json;
	*length = json_length;

	return 0;
}

static bool json_read_vec3(const nx_json *nxj, const char *key, vec3 *out)
{
	const nx_json *member = nx_json_get(nxj, key);

	if (member->type != NX_JSON_ARRAY)
		return false;

	out->data[0] = nx_json_item(member, 0)->dbl_value;
	out->data[1] = nx_json_item(member, 1)->dbl_value;
	out->data[2] = nx_json_item(member, 2)->dbl_value;

	return true;
}

static int rift_touch_parse_calibration(char *json,
		RiftTouchCalibration *c)
{
	const nx_json* nxj, *obj, *version, *array;
	int version_number = -1;
	unsigned int i;

	nxj = nx_json_parse(json, NULL);
	if (nxj == NULL)
		return -1;

	obj = nx_json_get(nxj, "TrackedObject");
	if (obj->type == NX_JSON_NULL)
		goto fail;

	version = nx_json_get(obj, "JsonVersion");
	if (version->type != NX_JSON_INTEGER || version->int_value != 2) {
		version_number = version->int_value;
		goto fail;
	}
	version_number = version->int_value;

	if (!json_read_vec3(obj, "ImuPosition", &c->imu_position))
		goto fail;

	c->joy_x_range_min = nx_json_get(obj, "JoyXRangeMin")->int_value;
	c->joy_x_range_max = nx_json_get(obj, "JoyXRangeMax")->int_value;
	c->joy_x_dead_min = nx_json_get(obj, "JoyXDeadMin")->int_value;
	c->joy_x_dead_max = nx_json_get(obj, "JoyXDeadMax")->int_value;
	c->joy_y_range_min = nx_json_get(obj, "JoyYRangeMin")->int_value;
	c->joy_y_range_max = nx_json_get(obj, "JoyYRangeMax")->int_value;
	c->joy_y_dead_min = nx_json_get(obj, "JoyYDeadMin")->int_value;
	c->joy_y_dead_max = nx_json_get(obj, "JoyYDeadMax")->int_value;

	c->trigger_min_range = nx_json_get(obj, "TriggerMinRange")->int_value;
	c->trigger_mid_range = nx_json_get(obj, "TriggerMidRange")->int_value;
	c->trigger_max_range = nx_json_get(obj, "TriggerMaxRange")->int_value;

	array = nx_json_get(obj, "GyroCalibration");
	for (i = 0; i < 12; i++)
		c->gyro_calibration[i] = nx_json_item(array, i)->dbl_value;

	c->middle_min_range = nx_json_get(obj, "MiddleMinRange")->int_value;
	c->middle_mid_range = nx_json_get(obj, "MiddleMidRange")->int_value;
	c->middle_max_range = nx_json_get(obj, "MiddleMaxRange")->int_value;

	c->middle_flipped = nx_json_get(obj, "MiddleFlipped")->int_value;

	array = nx_json_get(obj, "AccCalibration");
	for (i = 0; i < 12; i++)
		c->acc_calibration[i] = nx_json_item(array, i)->dbl_value;

	array = nx_json_get(obj, "CapSenseMin");
	for (i = 0; i < 8; i++)
		c->cap_sense_min[i] = nx_json_item(array, i)->int_value;

	array = nx_json_get(obj, "CapSenseTouch");
	for (i = 0; i < 8; i++)
		c->cap_sense_touch[i] = nx_json_item(array, i)->int_value;

	nx_json_free(nxj);
	return 0;
fail:
	LOGW("Unrecognised Touch Controller JSON data version %d\n%s\n", version_number, json);
	nx_json_free(nxj);
	return -1;
}

static int rift_touch_get_calibration(::hid_device *handle, int device_id,
		RiftTouchCalibration *calibration)
{
	uint8 hash[16];
	uint16 length;
	char *json = NULL;
	int ret = -1;

	ret = rift_radio_read_calibration_hash(handle, device_id, hash);
	if (ret < 0) {
		LOGV("Failed to read calibration hash from device %d", device_id);
		return ret;
	}

	ret = rift_radio_read_calibration(handle, device_id, &json, &length);
	if (ret < 0)
		return ret;

	rift_touch_parse_calibration(json, calibration);

	free(json);
	return 0;
}

static bool rift_hmd_radio_get_address(::hid_device *handle, uint8 radio_address[5])
{
	unsigned char buf[RIFT_FEATURE_BUFFER_SIZE];
	int ret_size;

	if (!rift_hmd_radio_send_cmd(handle, 0x05, 0x03, 0x05))
		return false;

	memset(buf, 0, RIFT_FEATURE_BUFFER_SIZE);
	buf[0] = (unsigned char)RIFT_CMD_RADIO_READ_DATA;
	ret_size = hid_get_feature_report(handle, buf, RIFT_FEATURE_BUFFER_SIZE);
	if (ret_size < 0)
		return false;

	if (!decode_radio_address(radio_address, buf, ret_size)) {
		LOGE("Failed to decode received radio address");
		return false;
	}

	return true;
}

static void vec3_from_rift_vec(const int32* smp, vec3* out_vec)
{
	out_vec->data[0] = (float)smp[0] * 0.0001f;
	out_vec->data[1] = (float)smp[1] * 0.0001f;
	out_vec->data[2] = (float)smp[2] * 0.0001f;
}

static void handle_tracker_sensor_msg(RiftHmd* priv, unsigned char* buffer, int size, RiftRevision rev)
{
	if (rev == REV_DK2 || rev == REV_CV1) {
		if (!decode_tracker_sensor_msg_dk2(&priv->sensor, buffer, size))
			return;
	}
	else {
		if (!decode_tracker_sensor_msg_dk1(&priv->sensor, buffer, size))
			return;
	}

	PktTrackerSensor* s = &priv->sensor;

	for (int i = 0; i < s->num_samples; i++) {
		vec3_from_rift_vec(s->samples[i].accel, &priv->raw_accel);
		vec3_from_rift_vec(s->samples[i].gyro, &priv->raw_gyro);

		// Calibration is done in the HMD, just multiply with gravity
		priv->raw_accel.data[0] *= OHMD_GRAVITY_EARTH;
		priv->raw_accel.data[1] *= OHMD_GRAVITY_EARTH;
		priv->raw_accel.data[2] *= OHMD_GRAVITY_EARTH;

		uint32 tick_delta = 1000;
		if (priv->last_imu_timestamp > 0)
			tick_delta = s->timestamp - priv->last_imu_timestamp;

		float dt = tick_delta * TICK_LEN;

		UpdateFusion(&priv->sensor_fusion, dt, &priv->raw_gyro, &priv->raw_accel, &priv->raw_mag);

		priv->last_imu_timestamp = s->timestamp;
	}
}

static void handle_radio_report(RiftHmd* priv, unsigned char* buffer, int size)
{
	PktRiftRadioReport report;
	if (!decode_rift_radio_report(&report, buffer, size))
		return;

	for (int i = 0; i < 2; i++) {
		PktRiftRadioMessage *m = &report.message[i];
		if (!m->valid)
			continue;

		if (m->device_type == RIFT_REMOTE) {
			priv->remote_buttons_state = m->remote.buttons;
		}
		else if (m->device_type == RIFT_TOUCH_CONTROLLER_LEFT || m->device_type == RIFT_TOUCH_CONTROLLER_RIGHT) {
			int idx = (m->device_type == RIFT_TOUCH_CONTROLLER_LEFT) ? 1 : 0;
			RiftTouchController *touch = &priv->touch_dev[idx];

			vec3 raw_accel, raw_gyro, raw_mag = {{0,0,0}};
			raw_accel.data[0] = (float)m->touch.accel[0] * 0.001f * OHMD_GRAVITY_EARTH;
			raw_accel.data[1] = (float)m->touch.accel[1] * 0.001f * OHMD_GRAVITY_EARTH;
			raw_accel.data[2] = (float)m->touch.accel[2] * 0.001f * OHMD_GRAVITY_EARTH;

			raw_gyro.data[0] = (float)m->touch.gyro[0] * 0.001f;
			raw_gyro.data[1] = (float)m->touch.gyro[1] * 0.001f;
			raw_gyro.data[2] = (float)m->touch.gyro[2] * 0.001f;

			float dt = 0.002f;
			if (touch->time_valid) {
				uint32 tick_delta = m->touch.timestamp - touch->last_timestamp;
				dt = tick_delta * 0.000001f;
			}
			touch->last_timestamp = m->touch.timestamp;
			touch->time_valid = true;

			UpdateFusion(&touch->imu_fusion, dt, &raw_gyro, &raw_accel, &raw_mag);

			touch->buttons = m->touch.buttons;
			touch->trigger = (float)m->touch.trigger / 1000.0f;
			touch->grip = (float)m->touch.grip / 1000.0f;
			touch->stick[0] = (float)((int)m->touch.stick[0] - 512) / 512.0f;
			touch->stick[1] = (float)((int)m->touch.stick[1] - 512) / 512.0f;
		}
	}
}

static void update_device(Device* device)
{
	RiftDevicePriv* dev = (RiftDevicePriv*)device;
	RiftHmd* priv = dev->hmd;
	RiftRevision rev = (RiftRevision)device->ctx->list.devices[device->active_device_idx].revision;

	unsigned char buffer[RIFT_FEATURE_BUFFER_SIZE];

	// Keep alive
	if (GetMonotonic(priv->ctx) - priv->last_keep_alive > (KEEP_ALIVE_VALUE / 2) * 0.001) {
		PktKeepAlive keep_alive = { 0, KEEP_ALIVE_VALUE };
		int size = encode_dk1_keep_alive(buffer, &keep_alive);
		if (send_feature_report(priv, buffer, size) == -1)
			LOGE("error sending keep alive");
		priv->last_keep_alive = GetMonotonic(priv->ctx);
	}

	// Read IMU
	while (true) {
		int size = hid_read(priv->handle, buffer, RIFT_FEATURE_BUFFER_SIZE);
		if (size < 0) {
			LOGE("error reading from device");
			return;
		}
		if (size == 0)
			break;

		if (buffer[0] == RIFT_IRQ_SENSORS_DK1 || buffer[0] == RIFT_IRQ_SENSORS_DK2) {
			handle_tracker_sensor_msg(priv, buffer, size, rev);
		}
		else if (buffer[0] == RIFT_RADIO_REPORT_ID) {
			handle_radio_report(priv, buffer, size);
		}
	}

	// Read Touch Radio if available
	if (priv->radio_handle) {
		while (true) {
			int size = hid_read(priv->radio_handle, buffer, RIFT_FEATURE_BUFFER_SIZE);
			if (size < 0) {
				LOGE("error reading from radio device");
				return;
			}
			if (size == 0)
				break;

			if (buffer[0] == RIFT_RADIO_REPORT_ID) {
				handle_radio_report(priv, buffer, size);
			}
		}
	}
}

static int getf(Device* device, FloatValue type, float* out)
{
	RiftDevicePriv* dev = (RiftDevicePriv*)device;
	RiftHmd* priv = dev->hmd;

	if (dev->id == 0) { // HMD
		switch (type) {
		case HMD_ROTATION_QUAT:
			*(quat*)out = priv->sensor_fusion.orient;
			return HMD_S_OK;

		case HMD_POSITION_VECTOR:
			*(vec3*)out = priv->imu.pos;
			return HMD_S_OK;

		case HMD_ACCELEROMETER_VECTOR:
			*(vec3*)out = priv->raw_accel;
			return HMD_S_OK;

		case HMD_GYROSCOPE_VECTOR:
			*(vec3*)out = priv->raw_gyro;
			return HMD_S_OK;

		case HMD_MAGNETOMETER_VECTOR:
			*(vec3*)out = priv->raw_mag;
			return HMD_S_OK;

		case HMD_CONTROLS_STATE:
			out[0] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_UP) ? 1.0f : 0.0f;
			out[1] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_DOWN) ? 1.0f : 0.0f;
			out[2] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_LEFT) ? 1.0f : 0.0f;
			out[3] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_RIGHT) ? 1.0f : 0.0f;
			out[4] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_OK) ? 1.0f : 0.0f;
			out[5] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_PLUS) ? 1.0f : 0.0f;
			out[6] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_MINUS) ? 1.0f : 0.0f;
			out[7] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_OCULUS) ? 1.0f : 0.0f;
			out[8] = (priv->remote_buttons_state & RIFT_REMOTE_BUTTON_BACK) ? 1.0f : 0.0f;
			return HMD_S_OK;

		default:
			return HMD_S_UNSUPPORTED;
		}
	}
	else { // Touch controllers
		int idx = dev->id - 1;
		RiftTouchController *touch = &priv->touch_dev[idx];
		switch (type) {
		case HMD_ROTATION_QUAT:
			*(quat*)out = touch->imu_fusion.orient;
			return HMD_S_OK;

		case HMD_POSITION_VECTOR:
			out[0] = out[1] = out[2] = 0.0f;
			return HMD_S_OK;

		case HMD_CONTROLS_STATE:
			out[0] = touch->trigger;
			out[1] = (touch->buttons & RIFT_TOUCH_CONTROLLER_BUTTON_A) ? 1.0f : 0.0f; // A or X
			out[2] = (touch->buttons & RIFT_TOUCH_CONTROLLER_BUTTON_B) ? 1.0f : 0.0f; // B or Y
			out[3] = (touch->buttons & RIFT_TOUCH_CONTROLLER_BUTTON_MENU) ? 1.0f : 0.0f; // Menu or Oculus
			out[4] = (touch->buttons & RIFT_TOUCH_CONTROLLER_BUTTON_STICK) ? 1.0f : 0.0f;
			out[5] = touch->stick[0];
			out[6] = touch->stick[1];
			out[7] = touch->grip;
			return HMD_S_OK;

		default:
			return HMD_S_UNSUPPORTED;
		}
	}
}

static void close_hmd(RiftHmd* hmd)
{
	if (hmd->leds)
		free(hmd->leds);

	if (hmd->radio_handle)
		hid_close(hmd->radio_handle);
	if (hmd->handle)
		hid_close(hmd->handle);
	free(hmd);
}

static int close_device(Device* device)
{
	RiftDevicePriv* dev = (RiftDevicePriv*)device;
	RiftHmd* hmd = dev->hmd;

	dev->opened = false;
	hmd->use_count--;

	if (hmd->use_count <= 0) {
		RiftDeviceList** current = &rift_hmds;
		while (*current) {
			if ((*current)->hmd == hmd) {
				RiftDeviceList* next = (*current)->next;
				free(*current);
				*current = next;
				break;
			}
			current = &((*current)->next);
		}
		close_hmd(hmd);
	}

	return HMD_S_OK;
}

static void rift_send_tracking_config(RiftHmd *priv, bool rift_s, uint16 exposure_us, uint16 period_us)
{
	unsigned char buf[RIFT_FEATURE_BUFFER_SIZE];
	PktTrackingConfig tracking_config;
	int size;

	memset(&tracking_config, 0, sizeof(tracking_config));
	tracking_config.command_id = 0;
	tracking_config.flags = RIFT_TRACKING_ENABLE | RIFT_TRACKING_AUTO_INCREMENT |
		RIFT_TRACKING_USE_CARRIER | RIFT_TRACKING_SYNC_INPUT | RIFT_TRACKING_VSYNC_LOCK;
	tracking_config.exposure_us = exposure_us;
	tracking_config.period_us = period_us;
	tracking_config.vsync_offset = RIFT_TRACKING_VSYNC_OFFSET;
	tracking_config.duty_cycle = RIFT_TRACKING_DUTY_CYCLE;

	size = encode_tracking_config(buf, &tracking_config);
	if (send_feature_report(priv, buf, size) == -1)
		LOGE("error sending tracking config");
}

static int rift_get_led_info(RiftHmd *priv)
{
	return 0; // Not fully implemented in OpenHMD rift.c either, just returns 0
}

static void init_touch_device(RiftTouchController* dev, int device_num, int device_type)
{
	memset(dev, 0, sizeof(RiftTouchController));
	dev->device_num = device_num;
	InitFusion(&dev->imu_fusion);
	
	dev->base.base.properties.control_count = 8;
	dev->base.base.properties.controls_hints[0] = HMD_TRIGGER;
	dev->base.base.properties.controls_hints[1] = HMD_BUTTON_A; // or X
	dev->base.base.properties.controls_hints[2] = HMD_BUTTON_B; // or Y
	dev->base.base.properties.controls_hints[3] = HMD_MENU;
	dev->base.base.properties.controls_hints[4] = HMD_ANALOG_PRESS;
	dev->base.base.properties.controls_hints[5] = HMD_ANALOG_X;
	dev->base.base.properties.controls_hints[6] = HMD_ANALOG_Y;
	dev->base.base.properties.controls_hints[7] = HMD_SQUEEZE;

	for (int i = 0; i < 8; i++) {
		dev->base.base.properties.controls_types[i] = (i == 0 || i == 5 || i == 6 || i == 7) ? HMD_ANALOG : HMD_DIGITAL;
	}
}

static RiftHmd* open_hmd(Driver* driver, DeviceDescription* desc)
{
	RiftHmd* priv = (RiftHmd*)calloc(1, sizeof(RiftHmd));
	if (!priv) return NULL;

	priv->ctx = driver->ctx;

	priv->handle = hid_open_path(desc->path);
	if (!priv->handle) {
		lhmd_set_error(driver->ctx, "Could not open %s", desc->path);
		free(priv);
		return NULL;
	}

	if (hid_set_nonblocking(priv->handle, 1) == -1) {
		lhmd_set_error(driver->ctx, "Failed to set non-blocking on device");
		hid_close(priv->handle);
		free(priv);
		return NULL;
	}

	if (desc->revision == REV_CV1) {
		struct hid_device_info* devs = hid_enumerate(OCULUS_VR_INC_ID, RIFT_CV1_PID);
		struct hid_device_info* cur_dev = devs;
		while (cur_dev) {
			if (cur_dev->interface_number == 1) {
				priv->radio_handle = hid_open_path(cur_dev->path);
				if (priv->radio_handle) break;
			}
			cur_dev = cur_dev->next;
		}
		hid_free_enumeration(devs);

		if (priv->radio_handle) {
			if (hid_set_nonblocking(priv->radio_handle, 1) == -1) {
				LOGE("Failed to set non-blocking on radio device");
			}
		}
	}

	unsigned char buf[RIFT_FEATURE_BUFFER_SIZE];
	int size;

	size = get_feature_report(priv, RIFT_CMD_RANGE, buf);
	decode_sensor_range(&priv->sensor_range, buf, size);

	size = get_feature_report(priv, RIFT_CMD_DISPLAY_INFO, buf);
	decode_sensor_display_info(&priv->display_info, buf, size);

	size = get_feature_report(priv, RIFT_CMD_SENSOR_CONFIG, buf);
	decode_sensor_config(&priv->sensor_config, buf, size);

	priv->coordinate_frame = priv->display_info.distortion_type != RIFT_DT_NONE ? RIFT_CF_HMD : RIFT_CF_SENSOR;

	SETFLAG(priv->sensor_config.flags, RIFT_SCF_USE_CALIBRATION, 1);
	SETFLAG(priv->sensor_config.flags, RIFT_SCF_AUTO_CALIBRATION, 1);

	// set_coordinate_frame implementation usually just sends a feature report
	// skipping it for now as it's complex and not strictly required for basic operation

	if (desc->revision == REV_CV1) {
		size = encode_enable_components(buf, true, true, true);
		send_feature_report(priv, buf, size);
		rift_send_tracking_config(priv, false, RIFT_TRACKING_EXPOSURE_US_CV1, RIFT_TRACKING_PERIOD_US_CV1);
		rift_hmd_radio_get_address(priv->handle, priv->radio_address);
	}
	else if (desc->revision == REV_DK2) {
		rift_send_tracking_config(priv, false, RIFT_TRACKING_EXPOSURE_US_DK2, RIFT_TRACKING_PERIOD_US_DK2);
	}

	// set keep alive interval to n seconds
	PktKeepAlive keep_alive = { 0, KEEP_ALIVE_VALUE };
	size = encode_dk1_keep_alive(buf, &keep_alive);

	send_feature_report(priv, buf, size);
	priv->last_keep_alive = GetMonotonic(priv->ctx);

	SetDefaultDeviceProperties(&priv->hmd_dev.base.properties);
	priv->hmd_dev.base.properties.hsize = priv->display_info.h_screen_size;
	priv->hmd_dev.base.properties.vsize = priv->display_info.v_screen_size;
	priv->hmd_dev.base.properties.hres = priv->display_info.h_resolution;
	priv->hmd_dev.base.properties.vres = priv->display_info.v_resolution;
	priv->hmd_dev.base.properties.lens_sep = priv->display_info.lens_separation;
	priv->hmd_dev.base.properties.lens_vpos = priv->display_info.v_center;
	priv->hmd_dev.base.properties.ratio = ((float)priv->display_info.h_resolution / (float)priv->display_info.v_resolution) / 2.0f;

	if (desc->revision == REV_CV1) {
		priv->hmd_dev.base.properties.control_count = 9;
		int hints[] = { HMD_BUTTON_Y, HMD_BUTTON_A, HMD_BUTTON_X, HMD_BUTTON_B, HMD_GENERIC, HMD_VOLUME_PLUS, HMD_VOLUME_MINUS, HMD_MENU, HMD_HOME };
		for (int i = 0; i < 9; i++) {
			priv->hmd_dev.base.properties.controls_hints[i] = hints[i];
			priv->hmd_dev.base.properties.controls_types[i] = HMD_DIGITAL;
		}
		init_touch_device(&priv->touch_dev[0], 0, RIFT_TOUCH_CONTROLLER_RIGHT);
		init_touch_device(&priv->touch_dev[1], 1, RIFT_TOUCH_CONTROLLER_LEFT);
	}

	switch (desc->revision) {
		case REV_DK2:
			SetUniversalDistortionK(&(priv->hmd_dev.base.properties), 0.247f, -0.145f, 0.103f, 0.795f);
			SetUniversalAberrationK(&(priv->hmd_dev.base.properties), 0.985f, 1.000f, 1.015f);
			break;
		case REV_DK1:
			SetUniversalDistortionK(&(priv->hmd_dev.base.properties), 1.003f, -1.005f, 0.403f, 0.599f);
			SetUniversalAberrationK(&(priv->hmd_dev.base.properties), 0.985f, 1.000f, 1.015f);
			break;
		case REV_CV1:
			SetUniversalDistortionK(&(priv->hmd_dev.base.properties), 0.098f, 0.324f, -0.241f, 0.819f);
			SetUniversalAberrationK(&(priv->hmd_dev.base.properties), 0.9952420f, 1.0f, 1.0008074f);
			priv->display_info.lens_separation = 0.054f;
			priv->hmd_dev.base.properties.lens_sep = priv->display_info.lens_separation;
			break;
	}

	CalculateDefaultProjectionMatrices(&priv->hmd_dev.base.properties);

	InitFusion(&priv->sensor_fusion);

	return priv;
}

static Device* Oculus_OpenDevice(Driver* driver, DeviceDescription* desc)
{
	RiftHmd* hmd = NULL;
	RiftDeviceList* cur = rift_hmds;
	while (cur) {
		if (strcmp(cur->path, desc->path) == 0) {
			hmd = cur->hmd;
			break;
		}
		cur = cur->next;
	}

	if (!hmd) {
		hmd = open_hmd(driver, desc);
		if (!hmd) return NULL;
		RiftDeviceList* entry = (RiftDeviceList*)malloc(sizeof(RiftDeviceList));
		strcpy(entry->path, desc->path);
		entry->hmd = hmd;
		entry->next = rift_hmds;
		rift_hmds = entry;
	}

	RiftDevicePriv* dev = NULL;
	if (desc->id == 0) dev = &hmd->hmd_dev;
	else if (desc->id == 1) dev = &hmd->touch_dev[0].base;
	else if (desc->id == 2) dev = &hmd->touch_dev[1].base;

	if (!dev) return NULL;

	hmd->use_count++;
	dev->hmd = hmd;
	dev->id = desc->id;
	dev->opened = true;

	dev->base.Update = update_device;
	dev->base.Close = close_device;
	dev->base.GetFloat = getf;

	return &dev->base;
}

static void Oculus_GetDeviceList(Driver* driver, DeviceList* list)
{
	RiftDevices rd[] = {
		{ "Rift (DK1)", OCULUS_VR_INC_ID, 0x0001, -1, REV_DK1 },
		{ "Rift (DK2)", OCULUS_VR_INC_ID, 0x0021, -1, REV_DK2 },
		{ "Rift (DK2)", OCULUS_VR_INC_ID, 0x2021, -1, REV_DK2 },
		{ "Rift (CV1)", OCULUS_VR_INC_ID, RIFT_CV1_PID, 0, REV_CV1 },
		{ "GearVR (Gen1)", SAMSUNG_ELECTRONICS_CO_ID, 0xa500, 0, REV_GEARVR_GEN1 },
	};

	for (int i = 0; i < 5; i++) {
		struct hid_device_info* devs = hid_enumerate(rd[i].company, rd[i].id);
		struct hid_device_info* cur_dev = devs;

		while (cur_dev) {
			if (rd[i].iface == -1 || cur_dev->interface_number == rd[i].iface) {
				DeviceDescription* desc = &list->devices[list->num_devices++];
				#ifdef flagDEBUG
				LOG("\tFound " << rd[i].name);
				#endif
				strcpy(desc->driver, "Oculus Rift Driver");
				strcpy(desc->vendor, "Oculus VR, Inc.");
				strcpy(desc->product, rd[i].name);
				desc->revision = rd[i].rev;
				desc->device_class = HMD_DEVICE_CLASS_HMD;
				desc->device_flags = HMD_DEVICE_FLAGS_ROTATIONAL_TRACKING;
				strcpy(desc->path, cur_dev->path);
				desc->driver_ptr = driver;
				desc->id = 0;

				if (rd[i].rev == REV_CV1) {
					// Add touch controllers
					DeviceDescription* touch_l = &list->devices[list->num_devices++];
					*touch_l = *desc;
					strcpy(touch_l->product, "Oculus Touch (Left)");
					touch_l->device_class = HMD_DEVICE_CLASS_CONTROLLER;
					touch_l->device_flags = HMD_DEVICE_FLAGS_ROTATIONAL_TRACKING | HMD_DEVICE_FLAGS_LEFT_CONTROLLER;
					touch_l->id = 2;

					DeviceDescription* touch_r = &list->devices[list->num_devices++];
					*touch_r = *desc;
					strcpy(touch_r->product, "Oculus Touch (Right)");
					touch_r->device_class = HMD_DEVICE_CLASS_CONTROLLER;
					touch_r->device_flags = HMD_DEVICE_FLAGS_ROTATIONAL_TRACKING | HMD_DEVICE_FLAGS_RIGHT_CONTROLLER;
					touch_r->id = 1;
				}
			}
			cur_dev = cur_dev->next;
		}
		hid_free_enumeration(devs);
	}
}

static void Oculus_Destroy(Driver* driver)
{
	free(driver);
}

Driver* CreateOculusDriver(Context* ctx)
{
	Driver* drv = (Driver*)calloc(1, sizeof(Driver));
	if (!drv) return NULL;

	drv->ctx = ctx;
	drv->GetDeviceList = Oculus_GetDeviceList;
	drv->OpenDevice = Oculus_OpenDevice;
	drv->Destroy = Oculus_Destroy;

	return drv;
}

NAMESPACE_HMD_END
