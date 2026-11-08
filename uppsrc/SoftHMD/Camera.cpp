#include "SoftHMD.h"
#include "Camera.h"
#include <Core/Core.h>
#include <libusb-1.0/libusb.h>

NAMESPACE_HMD_BEGIN

#define WMR_VID 0x045e
#define WMR_PID 0x0659
#define WMR_INTERFACE_CONTROL 0
#define WMR_VIDEO_INTERFACE 3
#define WMR_VIDEO_INTERFACE2 4
#define WMR_VIDEO_ENDPOINT 0x85
#define WMR_COMMAND_ENDPOINT 0x05
#define WMR_COMMAND_ENDPOINT2 0x06
#define WMR_BULK_SIZE 616538
#define WMR_PACKET_SIZE 0x6000
#define WMR_HEADER_SIZE 0x20

#define HOLOLENS_CAMERA2_MAGIC	0x2b6f6c44

#ifdef CPU_LE
#define CpuToLE32(x) (x)
#define CpuToLE16(x) (x)
#else
#define CpuToLE32(x) (SwapEndian(x))
#define CpuToLE16(x) (SwapEndian(x))
#endif

struct hololens_camera2_command {
	uint32 magic;
	uint32 len;
	uint32 command;
} __attribute__((packed));

struct hololens_camera2_unknown_command {
	uint32 magic;
	uint32 len;
	uint16 command;
	uint16 camera; /* 0 for left, 1 for right */
	uint16 unknown_6000;
	uint16 gain; /* observed 82 to 255 */
	uint16 second_camera; /* same as camera */
} __attribute__((packed));

static int wmr_camera_send(libusb_device_handle* usb_handle, void *buf, size_t len)
{
	int transferred = 0;
	int r = libusb_bulk_transfer(usb_handle, WMR_COMMAND_ENDPOINT, (byte*)buf, (int)len, &transferred, 1000);
	if(r != 0) {
		r = libusb_bulk_transfer(usb_handle, WMR_COMMAND_ENDPOINT2, (byte*)buf, (int)len, &transferred, 1000);
	}
	if(r != 0) {
		Cout() << "Camera command error: " << libusb_error_name(r) << " (" << r << "), len=" << (int)len << "\n";
	}
	return r;
}

static void wmr_camera_set_active(libusb_device_handle* usb_handle, bool active)
{
	struct hololens_camera2_command command;
	command.magic = CpuToLE32(HOLOLENS_CAMERA2_MAGIC);
	command.len = CpuToLE32(sizeof(command));
	command.command = CpuToLE32(active ? 0x81 : 0x82);

	wmr_camera_send(usb_handle, &command, sizeof(command));
}

static void wmr_camera_set_gain(libusb_device_handle* usb_handle, uint8 camera, uint8 gain)
{
	struct hololens_camera2_unknown_command command;
	command.magic = CpuToLE32(HOLOLENS_CAMERA2_MAGIC);
	command.len = CpuToLE32(sizeof(command));
	command.command = CpuToLE16(0x80);
	command.camera = CpuToLE16(camera);
	command.unknown_6000 = CpuToLE16(6000);
	command.gain = CpuToLE16(gain);
	command.second_camera = CpuToLE16(camera);

	wmr_camera_send(usb_handle, &command, sizeof(command));
}

Camera::Camera()
{
	opened = false;
	active_transfers = 0;
	usb_ctx = NULL;
	usb_handle = NULL;
	async_buffers = 8;
	transfer_timeout_ms = 0;
	last_halt_clear_usecs = 0;
	transfers.clear();
	skip_streak_bright = 0;
	skip_streak_dark = 0;
	gap_occurred = false;
	serial_counter = 0;
	verbose = false;
	stopping = false;
	resubmitting = false;
}

Camera::~Camera()
{
	Close();
}

void LIBUSB_CALL Camera::TransferCallback(struct libusb_transfer* xfer)
{
	Transfer* t = (Transfer*)xfer->user_data;
	Camera* cam = t->camera;
	const int status = (int)xfer->status;
	
	// Lock 'mutex' only for stats updates
	const bool locked = cam->mutex.TryEnter();
	if(locked) {
		if(status >= 0 && status <= LIBUSB_TRANSFER_OVERFLOW)
			cam->stats.status_counts[status]++;
		
		if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
			if(xfer->actual_length > 0) {
				cam->stats.last_transferred = xfer->actual_length;
				if(xfer->actual_length < cam->stats.min_transferred)
					cam->stats.min_transferred = xfer->actual_length;
				if(xfer->actual_length > cam->stats.max_transferred)
					cam->stats.max_transferred = xfer->actual_length;
				cam->stats.last_error = 0;
				cam->stats.last_r = 0;
			}
		}
		else {
			cam->stats.last_error = xfer->status;
			cam->stats.last_r = xfer->status;
			cam->stats.usb_errors++;
			if(xfer->status == LIBUSB_TRANSFER_NO_DEVICE) cam->stats.no_device_errors++;
			if (xfer->status == LIBUSB_TRANSFER_TIMED_OUT) cam->stats.timeout_errors++;
			else if (xfer->status == LIBUSB_TRANSFER_OVERFLOW) cam->stats.overflow_errors++;
			else cam->stats.other_errors++;
		}
		cam->mutex.Leave();
	}
	
	// AppendRaw takes 'raw_mutex' - call it WITHOUT holding 'mutex' to avoid AB-BA deadlock
	if(xfer->status == LIBUSB_TRANSFER_COMPLETED && xfer->actual_length > 0) {
		if(cam->gap_occurred) {
			// A WMR frame end is marked by a short packet (616538 % 1024 = 90)
			// Any transfer ending with (len % 1024 != 0) is a frame boundary.
			if((xfer->actual_length % 1024) != 0) {
				cam->gap_occurred = false;
				if(cam->verbose) Cout() << "USB: Resynced after gap (len=" << xfer->actual_length << ")\n";
				// If we happen to get a full frame, keep it.
				if(xfer->actual_length == 616538)
					cam->AppendRaw(xfer->buffer, xfer->actual_length);
			}
			// Discard data while out of sync
		}
		else {
			cam->AppendRaw(xfer->buffer, xfer->actual_length);
		}
	}
	else if(xfer->status != LIBUSB_TRANSFER_COMPLETED) {
		if(cam->verbose) Cout() << "USB Error: Transfer status " << status << "\n";
		cam->gap_occurred = true;
	}
	
	// Final checks using flag only (no mutex needed for IsRunning)
	bool re_submitted = false;
	if(cam->resubmitting) {
		if(xfer->status == LIBUSB_TRANSFER_ERROR || xfer->status == LIBUSB_TRANSFER_STALL) {
			// Halt clear needs mutex for throttling
			if(cam->mutex.TryEnter()) {
				const int64 now = usecs();
				if(cam->last_halt_clear_usecs == 0 || now - cam->last_halt_clear_usecs >= 500000) {
					cam->last_halt_clear_usecs = now;
					cam->stats.halt_clear_attempts++;
					int ch = libusb_clear_halt(cam->usb_handle, WMR_VIDEO_ENDPOINT);
					if(ch != 0) {
						cam->stats.halt_clear_failures++;
						if(cam->verbose) Cout() << "USB Error: Failed to clear halt: " << libusb_error_name(ch) << " (" << ch << ")\n";
					}
					else if(cam->verbose) Cout() << "USB: Successfully cleared halt on video endpoint\n";
				}
				cam->mutex.Leave();
			}
		}
		
		bool allow_resubmit = true;
		if(xfer->status == LIBUSB_TRANSFER_NO_DEVICE || xfer->status == LIBUSB_TRANSFER_CANCELLED)
			allow_resubmit = false;
		
		if(allow_resubmit) {
			int r = libusb_submit_transfer(xfer);
			if (r == 0) {
				re_submitted = true;
			} else {
				if(cam->verbose) Cout() << "USB Error: Failed to resubmit transfer: " << libusb_error_name(r) << " (" << r << ")\n";
				if(cam->mutex.TryEnter()) {
					cam->stats.usb_errors++;
					cam->stats.last_r = r;
					cam->stats.resubmit_failures++;
					cam->mutex.Leave();
				}
			}
		} else {
			if(cam->mutex.TryEnter()) {
				cam->stats.resubmit_skips++;
				cam->mutex.Leave();
			}
		}
	}
	
	if(!re_submitted) {
		cam->active_transfers--;
	}
}

void HMD_APIENTRYDLL Camera::PopFrames(Vector<CameraFrame>& out)
{
	Upp::Mutex::Lock __(mutex);
	out = pick(queue);
}

void HMD_APIENTRYDLL Camera::PeakFrames(Vector<CameraFrame>& out)
{
	Upp::Mutex::Lock __(mutex);
	out <<= queue;
}

CameraStats HMD_APIENTRYDLL Camera::GetStats()
{
	Upp::Mutex::Lock __(mutex);
	return stats;
}

bool HMD_APIENTRYDLL Camera::Open()
{
	if(opened) return true;
	if(async_buffers <= 0) async_buffers = 1;
	if(mutex.TryEnter()) {
		stats.async_buffers = async_buffers;
		stats.transfer_timeout_ms = transfer_timeout_ms;
		mutex.Leave();
	}
	
	if(libusb_init(&usb_ctx) != 0) return false;
	
	usb_handle = libusb_open_device_with_vid_pid(usb_ctx, WMR_VID, WMR_PID);
	if(!usb_handle) {
		libusb_exit(usb_ctx);
		usb_ctx = NULL;
		return false;
	}
	
	for(int i : {0, 3, 4}) {
		libusb_detach_kernel_driver(usb_handle, i);
		if(libusb_claim_interface(usb_handle, i) != 0) {
			if(verbose) Cout() << "Failed to claim interface " << i << "\n";
		}
		libusb_set_interface_alt_setting(usb_handle, i, 0);
	}
	
	libusb_clear_halt(usb_handle, WMR_COMMAND_ENDPOINT);
	libusb_clear_halt(usb_handle, WMR_COMMAND_ENDPOINT2);
	libusb_clear_halt(usb_handle, WMR_VIDEO_ENDPOINT);
	
	wmr_camera_set_active(usb_handle, false);
	Upp::Sleep(200);
	wmr_camera_set_active(usb_handle, true);
	Upp::Sleep(200);
	wmr_camera_set_gain(usb_handle, 0, 0x80);
	wmr_camera_set_gain(usb_handle, 1, 0x80);
	Upp::Sleep(100);
	
	int effective_timeout_ms = transfer_timeout_ms;
	if(effective_timeout_ms <= 0)
		effective_timeout_ms = 1000;
	if(mutex.TryEnter()) {
		stats.transfer_timeout_ms = effective_timeout_ms;
		mutex.Leave();
	}
	
	transfers.assign(async_buffers, Transfer{});
	for(int i = 0; i < async_buffers; i++) {
		transfers[i].camera = this;
		transfers[i].buffer = (byte*)malloc(WMR_BULK_SIZE + 4096);
		transfers[i].libusb_xfer = libusb_alloc_transfer(0);
		libusb_fill_bulk_transfer(transfers[i].libusb_xfer, usb_handle, WMR_VIDEO_ENDPOINT,
		                          transfers[i].buffer, WMR_BULK_SIZE + 4096,
		                          TransferCallback, &transfers[i], effective_timeout_ms);
		libusb_submit_transfer(transfers[i].libusb_xfer);
	}
	active_transfers = async_buffers;

	resubmitting = true;
	usb_flag.Start();
	process_flag.Start();
	
	usb_thread.Start(THISBACK(Process));
	process_thread.Start(THISBACK(ProcessFrames));
	opened = true;
	
	return true;
}

void HMD_APIENTRYDLL Camera::Close()
{
	if(!opened) return;
	
	stopping = true;
	resubmitting = false; // Stop new resubmissions
	process_flag.Stop();
	
	for(int i = 0; i < transfers.size(); i++) {
		if(transfers[i].libusb_xfer)
			libusb_cancel_transfer(transfers[i].libusb_xfer);
	}
	
	// usb_thread will keep calling libusb_handle_events until all transfers are reaped
	int attempts = 0;
	while(active_transfers > 0 && attempts < 500) {
		Upp::Sleep(10);
		attempts++;
	}
	
	usb_flag.Stop();
	usb_thread.Wait();
	process_thread.Wait();
	
	if(usb_handle) {
		wmr_camera_set_active(usb_handle, false);
		for(int i : {0, 3, 4}) {
			libusb_release_interface(usb_handle, i);
			libusb_attach_kernel_driver(usb_handle, i);
		}
		libusb_close(usb_handle);
	}
	
	libusb_exit(usb_ctx);
	usb_ctx = NULL;
	
	for(int i = 0; i < transfers.size(); i++) {
		if(transfers[i].libusb_xfer)
			libusb_free_transfer(transfers[i].libusb_xfer);
		if(transfers[i].buffer)
			free(transfers[i].buffer);
		transfers[i].libusb_xfer = NULL;
		transfers[i].buffer = NULL;
	}
	transfers.clear();
	
	usb_handle = NULL;
	opened = false;
}

void Camera::Process()
{
	struct timeval tv = { 0, 10000 };
	int64 last_keepalive = usecs();
	int last_errors = 0;
	while(usb_flag.IsRunning()) {
		libusb_handle_events_timeout_completed(usb_ctx, &tv, NULL);
		
		int64 now = usecs();
		if(now - last_keepalive > 1000000) { // 1 second
			if(usb_handle) {
				// Refresh gain as a keep-alive
				wmr_camera_set_gain(usb_handle, 0, 0x80);
				wmr_camera_set_gain(usb_handle, 1, 0x80);
				
				// Recovery: if too many errors occurred, try to restart camera streaming
				if(stats.usb_errors > last_errors + 20) {
					if(verbose) Cout() << "USB: High error rate detected (" << (stats.usb_errors - last_errors) << " eps), restarting camera...\n";
					wmr_camera_set_active(usb_handle, false);
					Upp::Sleep(100);
					wmr_camera_set_active(usb_handle, true);
					gap_occurred = true;
				}
				last_errors = stats.usb_errors;
			}
			last_keepalive = now;
		}
	}
	usb_flag.SetStopped();
}

void Camera::ProcessFrames()
{
	while(process_flag.IsRunning() && !stopping) {
		if(!ProcessRawFrames())
			Upp::Sleep(1);
	}
	process_flag.SetStopped();
}

void Camera::AppendRaw(const byte* buffer, int size)
{
	if (size <= 0) return;
	
	RawDataBlock* block = NULL;
	{
		Upp::RWMutex::WriteLock __(raw_mutex);
		
		// Cleanup processed and NOT in-use items
		while(raw_queue.GetCount() > 0 && raw_queue.First().processed && raw_queue.First().in_use == 0)
			raw_queue.RemoveFirst();
			
		// Overflow protection
		if(raw_queue.GetCount() > 100) {
			if(verbose) Cout() << "Raw queue overflow, clearing to resync\n";
			// Note: clearing while items are in_use is dangerous if pointers are held.
			// But here consumer only holds reference to block inside the list.
			// If we clear the list, consumer's reference becomes invalid.
			// However, our logic ensures we only remove NOT in_use items above.
			// For a full Clear(), we should ideally wait or just risk it if it's an emergency.
			raw_queue.Clear();
			gap_occurred = true;
		}
		
		block = &raw_queue.Add();
		block->ready = false;
		block->processed = false;
		block->in_use = 0;
	}
	
	// Write data without lock
	block->data.SetCount(size);
	memcpy(block->data.Begin(), buffer, size);
	
	// Mark ready for consumer
	block->ready = true;
}

void Camera::AppendRawLocked(const byte* buffer, int size)
{
	// Deprecated/Unused
}

bool Camera::ProcessRawFrames()
{
	int64 start_usecs = usecs();
	bool processed_any = false;
	
	bool local_gap = gap_occurred.exchange(false);
	{
		Upp::RWMutex::WriteLock __(raw_mutex);
		
		while(raw_queue.GetCount() > 0 && raw_queue.First().processed && raw_queue.First().in_use == 0)
			raw_queue.RemoveFirst();
	}
	
	if(local_gap) {
		assembly_buffer.Clear();
	}

	{
		Upp::RWMutex::ReadLock __(raw_mutex);
		for(auto& b : raw_queue) {
			if(!b.ready) break;
			if(b.processed) continue;
			
			assembly_buffer.Append(b.data);
			b.processed = true;
			processed_any = true;
		}
	}
	
	if(!processed_any)
		return false;
	
	int local_ptr = assembly_buffer.GetCount();
	byte* local_data = assembly_buffer.Begin();
	bool processed = false;

	const int RAW_FRAME_SIZE = 616538;
	if(verbose && local_ptr >= RAW_FRAME_SIZE)
		Cout() << "HandleFrame: size=" << local_ptr << "\n";

	const int STRIPPED_FRAME_SIZE = 615706;
	static thread_local std::vector<byte> stripped;
	if((int)stripped.size() < STRIPPED_FRAME_SIZE)
		stripped.assign(STRIPPED_FRAME_SIZE, 0);
	
	int processed_bytes = 0;
	while(local_ptr >= RAW_FRAME_SIZE) {
		uint16 exposure = (local_data[32 + 6] << 8) | local_data[32 + 7];
		bool is_bright = (exposure != 0);
		
		if(verbose) Cout() << "  Frame found: exposure=" << exposure << ", is_bright=" << is_bright << ", balance=" << stats.bright_balance << "\n";

		bool skipped = false;
		bool balance_skip = false;
		{
			Upp::Mutex::Lock __(mutex);
			const int MAX_BALANCE = 5;
			if (is_bright && stats.bright_balance >= MAX_BALANCE) { skipped = true; balance_skip = true; }
			else if (!is_bright && stats.bright_balance <= -MAX_BALANCE) { skipped = true; balance_skip = true; }
			
			if (skipped) {
				stats.other_errors++; // Skip counter
				if(verbose) Cout() << "    Skipped (balance=" << stats.bright_balance << ")\n";
			}
		}
		
		if(balance_skip) {
			const int MAX_SKIP_STREAK = 5;
			if(is_bright) {
				skip_streak_bright++;
				if(skip_streak_bright >= MAX_SKIP_STREAK) {
					skip_streak_bright = 0;
					skipped = false;
					{ Upp::Mutex::Lock __(mutex); stats.bright_balance = 0; }
				}
			} else {
				skip_streak_dark++;
				if(skip_streak_dark >= MAX_SKIP_STREAK) {
					skip_streak_dark = 0;
					skipped = false;
					{ Upp::Mutex::Lock __(mutex); stats.bright_balance = 0; }
				}
			}
		} else {
			skip_streak_bright = 0;
			skip_streak_dark = 0;
		}
		
		if(!skipped) {
			processed = true;
			int sj = 0;
			for (int i = 0; i < RAW_FRAME_SIZE; i += WMR_PACKET_SIZE) {
				int payload_offset = i + WMR_HEADER_SIZE;
				int n = WMR_PACKET_SIZE - WMR_HEADER_SIZE;
				if (i + WMR_PACKET_SIZE > RAW_FRAME_SIZE) n = RAW_FRAME_SIZE - payload_offset;
				if(sj + n > STRIPPED_FRAME_SIZE)
					n = STRIPPED_FRAME_SIZE - sj;
				if (n > 0) {
					memcpy(stripped.data() + sj, local_data + payload_offset, n);
					sj += n;
				}
			}
			
			if(sj >= STRIPPED_FRAME_SIZE) {
				ImageBuffer ib(1280, 481);
				const byte* src = stripped.data() + 26;
				uint64 sum = 0;
				byte min_p = 255, max_p = 0;
				RGBA* dst = ib;
				for(int p = 0; p < 1280 * 481; p++) {
					byte v = src[p];
					dst[p] = GrayColor(v);
					sum += v;
					if(v < min_p) min_p = v;
					if(v > max_p) max_p = v;
				}
				
				{
					Upp::Mutex::Lock __(mutex);
					if(queue.GetCount() >= 4) queue.Remove(0); // Keep up to 4 frames
					CameraFrame& f = queue.Add();
					f.img = ib;
					f.is_bright = is_bright;
					f.exposure = exposure;
					f.serial = ++serial_counter;
					
					stats.frame_count++;
					if(is_bright) { stats.bright_frames++; stats.bright_balance++; } 
					else { stats.dark_frames++; stats.bright_balance--; }
					stats.bright_balance = Clamp(stats.bright_balance, -20, 20);
					stats.last_exposure = exposure;
					stats.avg_brightness = (double)sum / (1280 * 481);
					stats.min_pixel = min_p;
					stats.max_pixel = max_p;
					stats.handle_usecs = (int)(usecs() - start_usecs);
				}
			}
		}
		
		local_data += RAW_FRAME_SIZE;
		local_ptr -= RAW_FRAME_SIZE;
		processed_bytes += RAW_FRAME_SIZE;
	}
	
	if(processed_bytes > 0) {
		assembly_buffer.Remove(0, processed_bytes);
	}
	
	return processed;
}

NAMESPACE_HMD_END