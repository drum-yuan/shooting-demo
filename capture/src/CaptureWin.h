#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ddraw.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#ifdef DMF_MIRROR
#include "DisplayEsc.h"
#else
#include "IOCtl.h"
#endif
#include "Escapi.h"
#include "CaptureApi.h"


#define GRAB_TYPE_AUTO      0
#define GRAB_TYPE_MIRROR    1
#define GRAB_TYPE_DIRECTX   2
#define GRAB_TYPE_GDI       3

//镜像驱动抓屏
struct MIRROR_CAP
{
	BOOL             is_active;
	unsigned long	 index;
	DISPLAY_DEVICE   disp;
	time_t           last_check_drawbuf_time;
#ifdef DMF_MIRROR
	GETCHANGESBUF    buffer;
#else
	draw_buffer_t*   buffer;
#endif
};
// DX抓屏
struct D3D_CAP
{
	int                 cx;
	int                 cy;
	int                 bitcount;
	int                 line_bytes;
	int                 line_stride;

	int                       is_acquire_frame;
	ID3D11Device*             d11dev;
	ID3D11DeviceContext*      d11ctx;
	IDXGIOutputDuplication*   dxgi_dup;
	DXGI_OUTPUT_DESC          dxgi_desc;
	ID3D11Texture2D*          dxgi_text2d;
	IDXGISurface*             dxgi_surf;

	byte*                     buffer;
	byte*					  bak_buffer;
};
//GDI 抓屏
struct GDI_CAP
{
	int        cx;
	int        cy;
	int        line_bytes;
	int        line_stride;
	int        bitcount;
	HDC        memdc;
	HBITMAP    hbmp;
	byte*      buffer;
	byte*      back_buf;
};

struct ChangedRects
{
	unsigned short left;
	unsigned short top;
	unsigned short right;
	unsigned short bottom;
	char*  buffer;    //矩形框数据起始地址
};

struct T_BUFFER
{
	int    size;
	void*  buf;
};

typedef HRESULT(WINAPI *fnD3D11CreateDevice)(
	_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_Out_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_Out_opt_ ID3D11DeviceContext** ppImmediateContext);

class CCapture
{
public:
	CCapture(FrameCallback on_frame, void* param);
	~CCapture();
	void start_screen(int id, bool camera);
	void stop_screen();
	std::vector<CameraInfo> get_camera_list();
	void start_camera(int id);
	void stop_camera();
	void pause();
	void resume();
	void set_drop_interval(unsigned int count);
	unsigned int get_ack_sequence();
	void set_ack_sequence(unsigned int seq);
	unsigned int get_capture_sequence();
	void set_capture_sequence(unsigned int seq);
	void reset_sequence();
	void set_frame_rate(unsigned int rate);

	static DWORD CALLBACK LoopScreenProc(void* param);
	static DWORD CALLBACK LoopCameraProc(void* param);
	static LRESULT CALLBACK xDispWindowProc(HWND hwnd, UINT uMsg, WPARAM  wParam, LPARAM lParam);

private:
	void capture_mirror();
	void capture_dxgi();
	void capture_gdi();
	void capture_camera();
	BOOL init_mirror(BOOL is_init);
	BOOL init_dxgi();
	BOOL init_directx(BOOL is_init);
	BOOL init_gdi(BOOL is_init);
	BOOL init_camera(BOOL is_init);

	HWND create_messsage_window();
	void change_display(int w, int h, int bits);
#ifdef DMF_MIRROR
	void map_and_unmap_buffer(const char* dev_name, BOOL is_map, GETCHANGESBUF* p_buf);
#else
	void map_and_unmap_buffer(const char* dev_name, BOOL is_map, draw_buffer_t** p_buf);
#endif
	BOOL find_display_device(PDISPLAY_DEVICE disp, PDEVMODE mode, BOOL is_primary, int id);
	BOOL active_mirror_driver(BOOL is_active, PDISPLAY_DEVICE dp);
	LPSTR GetDispCode(INT code);
	void combine_rectangle(byte* primary_buffer, byte* second_buffer, int line_stride, LPRECT rect, LPSIZE screen,
		int bitcount, int SMALL_WIDTH, int SMALL_HEIGHT, HRGN& region);
	void region_to_rectangle(HRGN region, byte* dst_buf, int line_stride,
		int cx, int cy, int bitcount, ChangedRects** p_rc_array, int* p_rc_count);
	inline void tbuf_check(T_BUFFER* arr, int size)
	{
		if (!arr->buf || arr->size < size) {
			if (arr->buf) free(arr->buf);
			arr->buf = malloc(size);
			arr->size = size;
		}
	}

	int              m_GrabType;   // 抓屏方法: 0 自动选择，1 mirror，2 DX抓屏，3 GDI抓屏
	int				 m_MonitorID;
	int				 m_CameraID;
	char			 m_DispName[32];
	BOOL			 m_UseCamera;
	BOOL             m_Quit;
	BOOL             m_Pause;
	BOOL			 m_ForceFrame;
	MIRROR_CAP		 m_Mirror;
	D3D_CAP			 m_Directx;
	GDI_CAP	         m_GDI;
	long             m_SleepMsec;
	HWND             m_hMessageWnd;
	HANDLE           m_hThread;
	HANDLE           m_hEvent;
	T_BUFFER         m_tArr[2];
	HDESK            m_hdesk;
	SimpleCapParams  m_CamCapture;

	unsigned int	 m_IntervalCnt;
	unsigned int	 m_AckSeq;
	unsigned int	 m_CaptureSeq;
	FrameCallback	 onFrame;
	void*			 onframe_param;

	LARGE_INTEGER	 m_Counter;
	LARGE_INTEGER	 m_FrameBegin;
	LARGE_INTEGER	 m_FrameEnd;

	FILE*            m_fLog;
};

//小矩形块
#define GDI_SMALL_RECT_WIDTH    128
#define GDI_SMALL_RECT_HEIGHT   8

#define SAFE_RELEASE(a) if(a){ (a)->Release(); (a)=NULL; }
#define SAFE_FREE(a) if(a){ free(a); (a)=NULL; }

#ifdef DMF_MIRROR
#define MIRROR_DRIVER   "XieTong Mirror Display Driver"
#else
#define MIRROR_DRIVER   "XieTong Mirror Display Driver"
#define VIRTUAL_DRIVER  "XieTong Virtual Display Driver (XPDM for WinXP Only)"
#endif
