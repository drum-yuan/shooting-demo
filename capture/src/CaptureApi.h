#ifndef __CAPTURE_API_H__
#define __CAPTURE_API_H__

#include <vector>

#define CAPTURE_API __declspec(dllexport)

struct CallbackFrameInfo
{
	int width;
	int height;
	int line_bytes;
	int line_stride;
	int bitcount;
	int length;
	char* buffer;
};

struct CameraInfo
{
	int id;
	std::string name;
};

typedef void (*FrameCallback)(CallbackFrameInfo* frame, void* param);

CAPTURE_API void cap_initialize(FrameCallback on_frame, void* param);

CAPTURE_API void cap_uninitialize();

CAPTURE_API void cap_start_capture_screen(int monitor_id, bool camera);

CAPTURE_API void cap_stop_capture_screen();

CAPTURE_API std::vector<CameraInfo> cap_get_camera_device_list();

CAPTURE_API void cap_start_capture_camera(int camera_id);

CAPTURE_API void cap_stop_capture_camera();

CAPTURE_API void cap_pause_capture();

CAPTURE_API void cap_resume_capture();

CAPTURE_API void cap_set_drop_interval(unsigned int count);

CAPTURE_API unsigned int cap_get_ack_sequence();

CAPTURE_API void cap_set_ack_sequence(unsigned int seq);

CAPTURE_API unsigned int cap_get_capture_sequence();

CAPTURE_API void cap_set_capture_sequence(unsigned int seq);

CAPTURE_API void cap_reset_sequence();

CAPTURE_API void cap_set_frame_rate(unsigned int rate);

#endif
