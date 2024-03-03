#ifndef __CAPTURE_LINUX_H__
#define __CAPTURE_LINUX_H__

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/Xutil.h>
#include "CaptureApi.h"

class CCapture
{
public:
	CCapture(int monitor_id, FrameCallback on_frame, void* param);
	~CCapture();
	void start();
	void stop();
	void pause();
	void resume();
	void set_drop_interval(unsigned int count);
	void set_ack_sequence(unsigned int seq);
	unsigned int get_capture_sequence();
	void set_capture_sequence(unsigned int seq);
	void reset_sequence();
	void set_frame_rate(unsigned int rate);

	static void* __loop_msg(void* _p);

private:
    int xshm_init();
    bool xshm_reset();
    void xshm_uninit();
    bool check_resize();
	void capture_xshm();

    bool            xshm_valid;
	bool            quit;
    pthread_t       thread_id;
    pthread_cond_t  ready_cond;
	int		id;
	int             depth;
	int             width;
	int             height;
	int             x;
	int             y;
    Display*        display;
	XImage*         fb_image;
	Screen*         screen;
	Visual*         visual;
	Pixmap          fb_pixmap;
	Window          root_window;
	XShmSegmentInfo fb_shm_info;
	GC              xshm_gc;

	long            sleep_msec;
	bool            is_pause_grab;
	unsigned int	interval_count;
	unsigned int	ack_seq;
	unsigned int	capture_seq;
	FrameCallback	onFrame;
	void*			onframe_param;
};

#endif
