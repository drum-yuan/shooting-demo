#include "CaptureLinux.h"
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>

CCapture::CCapture(int monitor_id, FrameCallback on_frame, void* param)
{
    xshm_valid = true;
    quit = false;
    id = monitor_id;
    sleep_msec = 25;
    is_pause_grab = false;
    interval_count = 5;
	ack_seq = 0;
	capture_seq = 0;

	onFrame = on_frame;
	onframe_param = param;
}

CCapture::~CCapture()
{
}

void CCapture::start()
{
    pthread_attr_t attr;
    pthread_mutex_t mutex;

    pthread_cond_init(&ready_cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread_id, &attr, __loop_msg, this);
	pthread_cond_wait(&ready_cond, &mutex);

    pthread_attr_destroy(&attr);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&ready_cond);
}

void CCapture::stop()
{
    quit = true;
}

void CCapture::pause()
{
    is_pause_grab = true;
}

void CCapture::resume()
{
    is_pause_grab = false;
}

void CCapture::set_drop_interval(unsigned int count)
{
	interval_count = count;
}

void CCapture::set_ack_sequence(unsigned int seq)
{
	if (seq > ack_seq) {
		ack_seq = seq;
	}
}

unsigned int CCapture::get_capture_sequence()
{
	return capture_seq;
}

void CCapture::set_capture_sequence(unsigned int seq)
{
	capture_seq = seq;
}

void CCapture::reset_sequence()
{
	ack_seq = capture_seq;
}

void CCapture::set_frame_rate(unsigned int rate)
{
	if (rate > 100) {
		return;
	}
	sleep_msec = 1000 / rate;
}

void* CCapture::__loop_msg(void* _p)
{
    CCapture* cap = (CCapture*)_p;
    int ret = cap->xshm_init();
    if (ret < 0) {
        return NULL;
    } else if (ret == 0) {
        cap->xshm_valid = false;
    } else {
        cap->xshm_valid = true;
        printf("use xshm\n");
    }
    pthread_cond_signal(&cap->ready_cond);

    long sleep_msec = cap->sleep_msec;
    struct timespec ts;
    long long cost = 0;
    long long frame_begin = 0;
    long long frame_end = 0;
    while (!cap->quit) {
        sleep_msec = cap->sleep_msec;
        if (!cap->is_pause_grab) {
            while ((!cap->quit) && (cap->capture_seq - cap->ack_seq > cap->interval_count)) {
                usleep(10000);
            }
            if (cap->quit) {
                break;
            }
            clock_gettime(CLOCK_MONOTONIC, &ts);
            frame_begin = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            cap->check_resize();
            cap->capture_xshm();
            cap->capture_seq++;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            frame_end = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            cost = (frame_end > frame_begin) ? frame_end - frame_begin : 0;
            printf("frame cost %lld\n", cost);
            if (sleep_msec > cost) {
                sleep_msec -= cost;
            } else {
                sleep_msec = 0;
            }
        }
        usleep(sleep_msec * 1000);
    }
    cap->xshm_uninit();
    return NULL;
}

int CCapture::xshm_init()
{
    XVisualInfo tempvi;
	XVisualInfo* vi;
	XVisualInfo* vis;
	int vi_count;
	int i;
	Bool pixmaps;
	int major, minor;
	char env_val[4];

	sprintf(env_val, ":%d", id);
	if (!getenv("DISPLAY"))
		setenv("DISPLAY", env_val, 1);

	if (!XInitThreads())
		return -1;

    display = XOpenDisplay(NULL);
	if (!display) {
		printf("failed to open display: %s\n", XDisplayName(NULL));
		return -1;
	}
	screen = ScreenOfDisplay(display, 0);
	depth = DefaultDepthOfScreen(screen);
	width = WidthOfScreen(screen);
	height = HeightOfScreen(screen);
	root_window = RootWindow(display, 0);

	memset(&tempvi, 0, sizeof(tempvi));
	tempvi.c_class = TrueColor;
	tempvi.screen = 0;
	vis = XGetVisualInfo(display, VisualClassMask | VisualScreenMask, &tempvi, &vi_count);
	if (!vis) {
		printf("XGetVisualInfo failed\n");
		return -1;
	}
	for (i = 0; i < vi_count; i++) {
		vi = vis + i;
		if (vi->depth == depth)
		{
			visual = vi->visual;
			break;
		}
	}
	XFree(vis);
	XSelectInput(display, root_window, SubstructureNotifyMask);

	if (!XShmQueryExtension(display))
		return 0;
	if (!XShmQueryVersion(display, &major, &minor, &pixmaps))
		return 0;
	if (!pixmaps)
		return 0;
    if (!xshm_reset()) {
        return 0;
    }

    return 1;
}

bool CCapture::xshm_reset()
{
	XGCValues values;

	fb_shm_info.shmid = -1;
	fb_shm_info.shmaddr = (char*) - 1;
	fb_shm_info.readOnly = False;
	fb_image = XShmCreateImage(display, visual, depth, ZPixmap, NULL, &fb_shm_info, width, height);
	if (!fb_image) {
		printf("XShmCreateImage failed\n");
		return false;
	}
    fb_shm_info.shmid = shmget(IPC_PRIVATE, fb_image->bytes_per_line * fb_image->height, IPC_CREAT | 0600);
	if (fb_shm_info.shmid == -1) {
		printf("shmget failed\n");
		return false;
	}
	fb_shm_info.shmaddr = (char*)shmat(fb_shm_info.shmid, 0, 0);
	fb_image->data = fb_shm_info.shmaddr;
	if (fb_shm_info.shmaddr == ((char*) - 1)) {
		printf("shmat failed\n");
		return false;
	}
	if (!XShmAttach(display, &fb_shm_info))
		return false;
	XSync(display, False);
	shmctl(fb_shm_info.shmid, IPC_RMID, 0);
	fb_pixmap = XShmCreatePixmap(display, root_window, fb_image->data, &fb_shm_info,
                                fb_image->width, fb_image->height, fb_image->depth);
	XSync(display, False);
	if (!fb_pixmap)
		return false;
	values.subwindow_mode = IncludeInferiors;
	values.graphics_exposures = False;
	xshm_gc = XCreateGC(display, root_window, GCSubwindowMode | GCGraphicsExposures, &values);
	XSetFunction(display, xshm_gc, GXcopy);
	XSync(display, False);
	return true;
}

void CCapture::xshm_uninit()
{
	if (display) {
		XCloseDisplay(display);
		display = NULL;
	}
}

bool CCapture::check_resize()
{
	XWindowAttributes attr;
	XLockDisplay(display);
	XGetWindowAttributes(display, root_window, &attr);
	XUnlockDisplay(display);
	if (attr.width != width || attr.height != height) {
		width = attr.width;
		height = attr.height;
		x = attr.x;
		y = attr.y;
		if (xshm_valid) {
			xshm_reset();
		}
		return true;
	}

	return false;
}

void CCapture::capture_xshm()
{
    if (xshm_valid) {
        XCopyArea(display, root_window, fb_pixmap, xshm_gc, 0, 0, width, height, 0, 0);
    } else {
        fb_image = XGetImage(display, root_window, x, y, width, height, AllPlanes, ZPixmap);
    }

    CallbackFrameInfo frame;
    frame.grab_type = 1;
	frame.width = width;
	frame.height = height;
	frame.line_bytes = fb_image->bytes_per_line;
	frame.line_stride = fb_image->bytes_per_line / width * width;
	frame.bitcount = fb_image->bytes_per_line / width * 8;
	frame.buffer = fb_image->data;
	frame.length = fb_image->bytes_per_line * height;
	onFrame(&frame, onframe_param);
}
