#ifndef _SoftHMD_Camera_h_
#define _SoftHMD_Camera_h_

extern "C" {
#include <libusb-1.0/libusb.h>
}
#include <vector>
#include <atomic>

NAMESPACE_HMD_BEGIN

struct CameraFrame : public Moveable<CameraFrame> {
	Image img;
	bool is_bright;
	int exposure;
	int64 serial;
};

struct CameraStats {
	int frame_count;
	int bright_frames;
	int dark_frames;
	int last_exposure;
	int last_transferred;
	int last_error;
	int last_r;
	int min_transferred, max_transferred;
	double avg_brightness;
	byte min_pixel, max_pixel;
	
	int mutex_fails;
	int usb_errors;
	int timeout_errors;
	int overflow_errors;
	int other_errors;
	int no_device_errors;
	int resubmit_failures;
	int resubmit_skips;
	int halt_clear_attempts;
	int halt_clear_failures;
	int status_counts[LIBUSB_TRANSFER_OVERFLOW + 1];
	int async_buffers;
	int transfer_timeout_ms;
	
	int bright_balance; // bright_frames - dark_frames
	int handle_usecs;
	
	CameraStats() { 
		memset(this, 0, sizeof(*this)); 
		min_transferred = 1000000000;
	}
};

class Camera {
public:
	HMD_APIENTRYDLL Camera();
	HMD_APIENTRYDLL ~Camera();

	bool HMD_APIENTRYDLL Open();
	void HMD_APIENTRYDLL Close();
	
	bool HMD_APIENTRYDLL IsOpen() const { return opened; }
	
	void HMD_APIENTRYDLL PopFrames(Vector<CameraFrame>& out);
	void HMD_APIENTRYDLL PeakFrames(Vector<CameraFrame>& out);
	CameraStats HMD_APIENTRYDLL GetStats();
	void HMD_APIENTRYDLL SetVerbose(bool v) { verbose = v; }
	void HMD_APIENTRYDLL SetAsyncBuffers(int count) { async_buffers = count; }
	void HMD_APIENTRYDLL SetTransferTimeoutMs(int ms) { transfer_timeout_ms = ms; }
	
	typedef Camera CLASSNAME;

	static void LIBUSB_CALL TransferCallback(struct libusb_transfer* transfer);

private:
	void Process();
	void ProcessFrames();
	void AppendRaw(const byte* buffer, int size);
	void AppendRawLocked(const byte* buffer, int size);
	bool ProcessRawFrames();

	bool opened;
	RunningFlagSingle usb_flag;
	RunningFlagSingle process_flag;
	std::atomic<bool> resubmitting;
	bool verbose;
	std::atomic<int> active_transfers;
	std::atomic<bool> stopping;
	std::atomic<bool> gap_occurred;
	
	Upp::Thread usb_thread;
	Upp::Thread process_thread;
	Upp::Mutex mutex;
	Upp::RWMutex raw_mutex;
	Vector<CameraFrame> queue;
	int64 serial_counter;
	CameraStats stats;
	int skip_streak_bright;
	int skip_streak_dark;
	
	libusb_context* usb_ctx;
	libusb_device_handle* usb_handle;
	
	int async_buffers;
	struct Transfer {
		struct libusb_transfer* libusb_xfer;
		byte* buffer;
		Camera* camera;
	};
	std::vector<Transfer> transfers;
	int transfer_timeout_ms;
	
	struct RawDataBlock {
		Vector<byte> data;
		std::atomic<bool> ready;
		std::atomic<bool> processed;
		std::atomic<int> in_use;
		
		RawDataBlock() : ready(false), processed(false), in_use(0) {}
	};
	
	LinkedList<RawDataBlock, true> raw_queue;
	Vector<byte> assembly_buffer;
	int64 last_halt_clear_usecs;
};


NAMESPACE_HMD_END

#endif