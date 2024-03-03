#include "CaptureApi.h"
#include "CaptureWin.h"


static CCapture *s_pCapture = NULL;

void cap_initialize(FrameCallback on_frame, void* param)
{
	if (s_pCapture != NULL) {
		delete s_pCapture;
	}
	s_pCapture = new CCapture(on_frame, param);
}

void cap_uninitialize()
{
	if (s_pCapture != NULL) {
		delete s_pCapture;
		s_pCapture = NULL;
	}
}

void cap_start_capture_screen(int monitor_id, bool camera)
{
	if (s_pCapture != NULL) {
		s_pCapture->start_screen(monitor_id, camera);
	}
}

void cap_stop_capture_screen()
{
	if (s_pCapture != NULL) {
		s_pCapture->stop_screen();
	}
}

std::vector<CameraInfo> cap_get_camera_device_list()
{
	std::vector<CameraInfo> vecCamera;
	if (s_pCapture != NULL) {
		return s_pCapture->get_camera_list();
	}
	return vecCamera;
}

void cap_start_capture_camera(int camera_id)
{
	if (s_pCapture != NULL) {
		s_pCapture->start_camera(camera_id);
	}
}

void cap_stop_capture_camera()
{
	if (s_pCapture != NULL) {
		s_pCapture->stop_camera();
	}
}

void cap_pause_capture()
{
	if (s_pCapture != NULL) {
		s_pCapture->pause();
	}
}

void cap_resume_capture()
{
	if (s_pCapture != NULL) {
		s_pCapture->resume();
	}
}

void cap_set_drop_interval(unsigned int count)
{
	if (s_pCapture != NULL) {
		s_pCapture->set_drop_interval(count);
	}
}

unsigned int cap_get_ack_sequence()
{
	if (s_pCapture == NULL) {
		return 0;
	}
	return s_pCapture->get_ack_sequence();
}

void cap_set_ack_sequence(unsigned int seq)
{
	if (s_pCapture != NULL) {
		s_pCapture->set_ack_sequence(seq);
	}
}

unsigned int cap_get_capture_sequence()
{
	if (s_pCapture == NULL) {
		return 0;
	}
	return s_pCapture->get_capture_sequence();
}

void cap_set_capture_sequence(unsigned int seq)
{
	if (s_pCapture != NULL) {
		s_pCapture->set_capture_sequence(seq);
	}
}

void cap_reset_sequence()
{
	if (s_pCapture != NULL) {
		s_pCapture->reset_sequence();
	}
}

void cap_set_frame_rate(unsigned int rate)
{
	if (s_pCapture != NULL) {
		s_pCapture->set_frame_rate(rate);
	}
}