#include "CaptureWin.h"

static LPSTR dispCode[7] = {
	"Change Successful",
	"Must Restart",
	"Bad Flags",
	"Bad Parameters",
	"Failed",
	"Bad Mode",
	"Not Updated" };

CCapture::CCapture(FrameCallback on_frame, void* param)
{
	m_GrabType = 0;
	m_MonitorID = -1;
	m_CameraID = 0;
	onFrame = on_frame;
	onframe_param = param;

	m_UseCamera = FALSE;
	m_Quit = FALSE;
	m_Pause = FALSE;
	m_ForceFrame = FALSE;

	memset(&m_Mirror, 0, sizeof(m_Mirror));
	memset(&m_Directx, 0, sizeof(m_Directx));
	memset(&m_GDI, 0, sizeof(m_GDI));
	
	m_SleepMsec = 25;
	m_hMessageWnd = NULL;
	m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThread = NULL;
	m_hdesk = NULL;

	m_IntervalCnt = 5;
	m_AckSeq = 0;
	m_CaptureSeq = 0;
	m_fLog = NULL;

	QueryPerformanceFrequency(&m_Counter);
	m_fLog = fopen("capture.log", "wb");
}

CCapture::~CCapture()
{
	if (m_fLog) {
		fclose(m_fLog);
	}
}

void CCapture::start_screen(int id, bool camera)
{
	m_MonitorID = id;
	m_UseCamera = camera;

	m_hThread = CreateThread(NULL, 1 * 1024 * 1024, LoopScreenProc, this, 0, NULL);
	if (!m_hThread) {
		CloseHandle(m_hEvent);
		return;
	}
	SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST); //提升优先级
	WaitForSingleObject(m_hEvent, INFINITE);
	ResetEvent(m_hEvent); 
	if (!m_hMessageWnd) {
		CloseHandle(m_hEvent); 
	}
}

void CCapture::stop_screen()
{
	m_Quit = TRUE;
	if (m_hMessageWnd) {
		PostMessage((HWND)m_hMessageWnd, WM_CLOSE, 0, 0);
		m_hMessageWnd = NULL;
	}
	if (WaitForSingleObject(m_hThread, 5000) == WAIT_OBJECT_0) {
		CloseHandle(m_hThread);
	}
}

std::vector<CameraInfo> CCapture::get_camera_list()
{
	std::vector<CameraInfo> vecCamera;
	int count = countCaptureDevices();
	int i = 0;
	for (i = 0; i < count; i++) {
		CameraInfo info;
		info.id = i;
		char name[256];
		getCaptureDeviceName(i, name, sizeof(name));
		info.name = std::string(name);
		vecCamera.push_back(info);
	}
	return vecCamera;
}

void CCapture::start_camera(int id)
{
	m_CameraID = id;
	m_hThread = CreateThread(NULL, 1 * 1024 * 1024, LoopCameraProc, this, 0, NULL);
	if (!m_hThread) {
		CloseHandle(m_hEvent);
		return;
	}
	SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST); //提升优先级
	WaitForSingleObject(m_hEvent, INFINITE);
	ResetEvent(m_hEvent);
	CloseHandle(m_hEvent);
}

void CCapture::stop_camera()
{
	m_Quit = TRUE;
	if (WaitForSingleObject(m_hThread, 5000) == WAIT_OBJECT_0) {
		CloseHandle(m_hThread);
	}
}

void CCapture::pause()
{
	m_Pause = TRUE;
}

void CCapture::resume()
{
	m_Pause = FALSE;
}

DWORD CALLBACK CCapture::LoopScreenProc(void* param)
{
	CCapture* capture = (CCapture*)param;
	CoInitialize(0);

	int grabType = capture->m_GrabType;
	HWND hwnd = capture->create_messsage_window();
	if (!hwnd) {
		capture->m_hMessageWnd = NULL;
		SetEvent(capture->m_hEvent);
		CoUninitialize();
		return 0;
	}

	BOOL success = FALSE;
	if (grabType == GRAB_TYPE_AUTO) {
		success = capture->init_mirror(TRUE);
		if (success) grabType = GRAB_TYPE_MIRROR;
		else {
			success = capture->init_directx(TRUE);
			if (success) grabType = GRAB_TYPE_DIRECTX;
			else {
				success = capture->init_gdi(TRUE);
				if (success) grabType = GRAB_TYPE_GDI;
			}
		}
		capture->m_GrabType = grabType;
	}
	else if (grabType == GRAB_TYPE_MIRROR) {
		success = capture->init_mirror(TRUE);
	}
	else if (grabType == GRAB_TYPE_DIRECTX) {
		success = capture->init_directx(TRUE);
	}
	else if (grabType == GRAB_TYPE_GDI) {
		success = capture->init_gdi(TRUE);
	}
	if (success && capture->m_UseCamera) {
		success = capture->init_camera(TRUE);
		doCapture(capture->m_CameraID);
	}

	if (!success) {
		fprintf(capture->m_fLog, "Can not init capture screen. type=%d\n", grabType);
		capture->m_hMessageWnd = NULL;
		SetEvent(capture->m_hEvent);
		CoUninitialize();
		return 0;
	}
	else {
		fprintf(capture->m_fLog, "Init capture screen success. type=%d\n", grabType);
	}
	SetEvent(capture->m_hEvent);

	MSG msg;
	BOOL bQuit = FALSE;
	unsigned int last_capture_seq = capture->m_CaptureSeq;
	while (!capture->m_Quit) {
		QueryPerformanceCounter(&capture->m_FrameBegin);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				bQuit = TRUE;
				break;
			}
			else {
				DispatchMessage(&msg);
			}
		}
		if (bQuit)break;
		if (!capture->m_Pause) {
			while ((!capture->m_Quit) && (capture->m_CaptureSeq > capture->m_AckSeq + capture->m_IntervalCnt)) {
				//printf("Sequence D-value %d\n", capture->m_CaptureSeq - capture->m_AckSeq);
				SleepEx(10, TRUE);
			}
			if (capture->m_Quit) {
				goto End;
			}

			if (grabType == GRAB_TYPE_MIRROR) {
				capture->capture_mirror();
			}
			else if (grabType == GRAB_TYPE_DIRECTX) {
				capture->capture_dxgi();
			}
			else if (grabType == GRAB_TYPE_GDI) {
				capture->capture_gdi();
			}
			if (capture->m_CaptureSeq > last_capture_seq) {
				last_capture_seq = capture->m_CaptureSeq;
				QueryPerformanceCounter(&capture->m_FrameEnd);
				LONGLONG dt = (capture->m_FrameEnd.QuadPart - capture->m_FrameBegin.QuadPart) * 1000 / capture->m_Counter.QuadPart;
				if (dt < capture->m_SleepMsec) {
					SleepEx(capture->m_SleepMsec - dt, TRUE);
					//printf("Sleep %d ms\n", capture->m_SleepMsec - dt);
				}
			}
			else {
				SleepEx(1, TRUE);
			}
		}
		else {
			SleepEx(capture->m_SleepMsec, TRUE);
		}
	}

End:
	CloseHandle(capture->m_hEvent);
	if (capture->m_UseCamera) {
		capture->init_camera(FALSE);
	}
	switch (capture->m_GrabType) {
	case GRAB_TYPE_MIRROR:  capture->init_mirror(FALSE);  break;
	case GRAB_TYPE_DIRECTX: capture->init_directx(FALSE); break;
	case GRAB_TYPE_GDI:     capture->init_gdi(FALSE);     break;
	}

	CoUninitialize();
	return 0;
}

DWORD CALLBACK CCapture::LoopCameraProc(void* param)
{
	CCapture* capture = (CCapture*)param;
	CoInitialize(0);

	BOOL success = capture->init_camera(TRUE);
	if (!success) {
		fprintf(capture->m_fLog, "Can not init capture camera\n");
		SetEvent(capture->m_hEvent);
		CoUninitialize();
		return 0;
	}
	else {
		fprintf(capture->m_fLog, "Init capture camera\n");
	}
	SetEvent(capture->m_hEvent);

	doCapture(capture->m_CameraID);
	BOOL bQuit = FALSE;
	unsigned int last_capture_seq = capture->m_CaptureSeq;
	while (!capture->m_Quit) {
		QueryPerformanceCounter(&capture->m_FrameBegin);
		if (!capture->m_Pause) {
			while ((!capture->m_Quit) && (capture->m_CaptureSeq > capture->m_AckSeq + capture->m_IntervalCnt)) {
				//printf("Sequence D-value %d\n", capture->m_CaptureSeq - capture->m_AckSeq);
				SleepEx(10, TRUE);
			}
			if (capture->m_Quit) {
				goto End;
			}

			capture->capture_camera();
			if (capture->m_CaptureSeq > last_capture_seq) {
				last_capture_seq = capture->m_CaptureSeq;
				QueryPerformanceCounter(&capture->m_FrameEnd);
				LONGLONG dt = (capture->m_FrameEnd.QuadPart - capture->m_FrameBegin.QuadPart) * 1000 / capture->m_Counter.QuadPart;
				if (dt < capture->m_SleepMsec) {
					SleepEx(capture->m_SleepMsec - dt, TRUE);
					//printf("Sleep %d ms\n", capture->m_SleepMsec - dt);
				}
			}
			else {
				SleepEx(1, TRUE);
			}
		}
		else {
			SleepEx(capture->m_SleepMsec, TRUE);
		}
	}
End:
	CloseHandle(capture->m_hEvent);
	capture->init_camera(FALSE);
	CoUninitialize();
	return 0;
}

LRESULT CALLBACK CCapture::xDispWindowProc(HWND hwnd, UINT uMsg, WPARAM  wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DISPLAYCHANGE:
	{
		CCapture* dp = (CCapture*)GetProp(hwnd, "xDispData");
		if (dp) {
			if (dp->m_GrabType == GRAB_TYPE_MIRROR) {
				dp->change_display(LOWORD(lParam), HIWORD(lParam), wParam);
			}
			else if (dp->m_GrabType == GRAB_TYPE_DIRECTX) {
				dp->init_directx(FALSE);
				dp->init_directx(TRUE);
			}
			else if (dp->m_GrabType == GRAB_TYPE_GDI) {
				dp->init_gdi(FALSE);
				dp->init_gdi(TRUE);
			}
		}
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND CCapture::create_messsage_window()
{
	HMODULE hmod = GetModuleHandle(NULL);

	const char* cls_name = "xdisp_window_class";
	WNDCLASS wndClass = { 0,
		xDispWindowProc,
		0, 0,
		hmod,
		NULL,NULL,NULL,NULL,
		cls_name
	};
	RegisterClass(&wndClass);
	HWND hwnd = CreateWindow(cls_name, "", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, NULL, NULL, hmod, NULL);
	m_hMessageWnd = hwnd;
	if (!hwnd) {
		fprintf(m_fLog, "NULL Message Window.\n");
	}
	SetProp(hwnd, "xDispData", this);

	return hwnd;
}

void CCapture::change_display(int w, int h, int bits)
{
	fprintf(m_fLog, "change new width=%d, heigt=%d, bitcount=%d\n", w, h, bits);
#ifdef DMF_MIRROR
	if (m_Mirror.buffer.Userbuffer) {
#else
	if (m_Mirror.buffer) {
#endif
		map_and_unmap_buffer(m_Mirror.disp.DeviceName, FALSE, &m_Mirror.buffer);
		m_Mirror.last_check_drawbuf_time = 0;
#ifdef DMF_MIRROR
		m_Mirror.buffer.Userbuffer = NULL;
#else
		m_Mirror.buffer = NULL;
#endif
		m_Mirror.index = -1;
	}

	DEVMODE devmode;
	memset(&devmode, 0, sizeof(DEVMODE));
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmDriverExtra = 0;
	BOOL bRet = EnumDisplaySettings(m_DispName, ENUM_CURRENT_SETTINGS, &devmode);
	devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	strcpy((char*)devmode.dmDeviceName, "xietong_mirror");
	devmode.dmPelsWidth = w;
	devmode.dmPelsHeight = h;
	devmode.dmBitsPerPel = bits;

	INT code = ChangeDisplaySettingsEx(m_Mirror.disp.DeviceName, &devmode, NULL, CDS_UPDATEREGISTRY, NULL);
	fprintf(m_fLog, "Change_display Update Registry on device mode: %s\n", GetDispCode(code));
}

void CCapture::set_drop_interval(unsigned int count)
{
	m_IntervalCnt = count;
}

unsigned int CCapture::get_ack_sequence()
{
	return m_AckSeq;
}

void CCapture::set_ack_sequence(unsigned int seq)
{
	if (seq > m_AckSeq) {
		m_AckSeq = seq;
	}
}

unsigned int CCapture::get_capture_sequence()
{
	return m_CaptureSeq;
}

void CCapture::set_capture_sequence(unsigned int seq)
{
	m_CaptureSeq = seq;
}

void CCapture::reset_sequence()
{
	m_AckSeq = m_CaptureSeq;
	m_ForceFrame = TRUE;
}

void CCapture::set_frame_rate(unsigned int rate)
{
	if (rate > 100) {
		return;
	}
	m_SleepMsec = 1000 / rate;
}

#ifdef DMF_MIRROR
void CCapture::map_and_unmap_buffer(const char* dev_name, BOOL is_map, GETCHANGESBUF* p_buf)
{
	fprintf(m_fLog, "buffer map %d\n", is_map);
	DWORD code = dmf_esc_usm_pipe_unmap;
	if (is_map) {
		code = dmf_esc_usm_pipe_map;
	}
#else
void CCapture::map_and_unmap_buffer(const char* dev_name, BOOL is_map, draw_buffer_t** p_buf)
{
	DWORD code = ESCAPE_CODE_UNMAP_USERBUFFER;
	if (is_map) {
		code = ESCAPE_CODE_MAP_USERBUFFER;
	}
#endif

	HDC hdc = CreateDC(NULL, dev_name, NULL, NULL);
	if (!hdc) {
		fprintf(m_fLog, "CreateDC err=%d\n", GetLastError());
		return;
	}

#ifdef DMF_MIRROR
	int ret;
	if (is_map) {
		ret = ExtEscape(hdc, code, 0, NULL, sizeof(GETCHANGESBUF), (LPSTR)p_buf);
		fprintf(m_fLog, "Map ExtEscape ret=%d, err=%d\n", ret, GetLastError());
		if (ret <= 0) {
			
			DeleteDC(hdc);
			return;
		}
	}
	else {
		ret = ExtEscape(hdc, code, sizeof(GETCHANGESBUF), (LPCSTR)p_buf, 0, NULL);
		fprintf(m_fLog, "UnMap ExtEscape ret=%d, err=%d\n", ret, GetLastError());
		if (ret <= 0) {
			
			DeleteDC(hdc);
			return;
		}
	}
#else
	unsigned __int64  u_addr = 0;
	int ret;
	if (is_map) {
		ret = ExtEscape(hdc, code, 0, NULL, sizeof(u_addr), (LPSTR)&u_addr);
		if (ret <= 0) {
			printf("Map ExtEscape ret=%d, err=%d\n", ret, GetLastError());
			DeleteDC(hdc);
			return;
		}
		*p_buf = (draw_buffer_t*)(ULONG_PTR)u_addr;
		printf("-- map user=%p\n", *p_buf);
	}
	else {
		u_addr = (ULONG_PTR)*p_buf;
		ret = ExtEscape(hdc, code, sizeof(u_addr), (LPCSTR)&u_addr, 0, NULL);
		if (ret <= 0) {
			printf("UnMap ExtEscape err=%d\n", GetLastError());
			DeleteDC(hdc);
			return;
		}
	}
#endif
	DeleteDC(hdc);
}

BOOL CCapture::find_display_device(PDISPLAY_DEVICE disp, PDEVMODE mode, BOOL is_primary, int id)
{
	DISPLAY_DEVICE dispDevice;
	memset(&dispDevice, 0, sizeof(DISPLAY_DEVICE));
	dispDevice.cb = sizeof(DISPLAY_DEVICE);
	int num = 0; 
	BOOL find = FALSE;

	if (id >= 0) {
		if (EnumDisplayDevices(NULL, id, &dispDevice, NULL)) {
			find = TRUE;
		}
	}
	else {
		while (EnumDisplayDevices(NULL, num, &dispDevice, NULL)) {
			/*printf("devName=[%s], desc=[%s], id=[%s], key[%s],flags=0x%X\n",
				dispDevice.DeviceName, dispDevice.DeviceString, dispDevice.DeviceID, dispDevice.DeviceKey, dispDevice.StateFlags);*/
			if (is_primary) {
				if (dispDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {//主显示器
					find = TRUE;
					break;
				}
			}
			else {
				const char* desc = MIRROR_DRIVER;
#if (WINVER < 0x0601)
				desc = VIRTUAL_DRIVER;
#endif
				if (_stricmp(dispDevice.DeviceString, desc) == 0) {
					find = TRUE;
					break;
				}
			}
			++num;
		}
	}
	if (!find) {
		return FALSE;
	}
	BOOL bRet = FALSE;
	if (id >= 0) {
		strcpy_s(m_DispName, dispDevice.DeviceName);
		bRet = EnumDisplaySettings(m_DispName, ENUM_CURRENT_SETTINGS, mode);
	}
	else {
		bRet = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, mode);
	}
	if (!bRet) {
		fprintf(m_fLog, "not get current display information\n");
		return FALSE;
	}

	*disp = dispDevice;
	fprintf(m_fLog, "Found devName=[%s], desc=[%s], id=[%s], key[%s], flags=0x%X\n", 
		dispDevice.DeviceName, dispDevice.DeviceString, dispDevice.DeviceID, dispDevice.DeviceKey, dispDevice.StateFlags);
	return TRUE;
}

//激活或者取消镜像驱动
BOOL CCapture::active_mirror_driver(BOOL is_active, PDISPLAY_DEVICE dp)
{
	DISPLAY_DEVICE dispDevice;
	PDISPLAY_DEVICE disp;
	DEVMODE devmode;

	memset(&devmode, 0, sizeof(DEVMODE));
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmDriverExtra = 0;
	if (is_active || !dp) {
		BOOL b = find_display_device(&dispDevice, &devmode, FALSE, -1);
		if (!b) {
			fprintf(m_fLog, "not found Mirror Driver maybe not install.\n");
			return FALSE;
		}
		disp = &dispDevice;
	}
	else {
		disp = dp;
	}

	DWORD attach = 0;
	if (is_active) attach = 1;
	HKEY hkey;
	if (RegOpenKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\xdisp_mirror\\Device0", &hkey) == ERROR_SUCCESS) {
		RegSetValueEx(hkey, "Attach.ToDesktop", 0, REG_DWORD, (BYTE*)&attach, sizeof(DWORD));
		RegCloseKey(hkey);
	}
	char keystr[1024];
	char* sep = strstr(disp->DeviceKey, "\\{");
	if (!sep) {
		fprintf(m_fLog, "invalid EnumDisplaySettings param.\n");
		return FALSE;
	}

	sprintf(keystr, "SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Control\\VIDEO%s", sep);
	if (RegOpenKey(HKEY_LOCAL_MACHINE, keystr, &hkey) == ERROR_SUCCESS) {
		RegSetValueEx(hkey, "Attach.ToDesktop", 0, REG_DWORD, (BYTE*)&attach, sizeof(DWORD));
		RegCloseKey(hkey);
	}

	if (!is_active) {
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);
		devmode.dmPelsWidth = 0;
		devmode.dmPelsHeight = 0;
	}
	else {
		strcpy((char*)devmode.dmDeviceName, "xietong_mirror");
		devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	}

	const char* DeviceName = disp->DeviceName;
	INT code = ChangeDisplaySettingsEx(DeviceName, &devmode, NULL, CDS_UPDATEREGISTRY, NULL);
	fprintf(m_fLog, "Active_mirror Update Registry on device mode: %s\n", GetDispCode(code));

	if (dp && disp != dp) {
		*dp = *disp;
	}
	return TRUE;
}

LPSTR CCapture::GetDispCode(INT code)
{
	switch (code) {
	case DISP_CHANGE_SUCCESSFUL: return dispCode[0];
	case DISP_CHANGE_RESTART: return dispCode[1];
	case DISP_CHANGE_BADFLAGS: return dispCode[2];
	case DISP_CHANGE_BADPARAM: return dispCode[3];
	case DISP_CHANGE_FAILED: return dispCode[4];
	case DISP_CHANGE_BADMODE: return dispCode[5];
	case DISP_CHANGE_NOTUPDATED: return dispCode[6];
	default:
		static char tmp[MAX_PATH];
		sprintf(&tmp[0], "Unknown code: %08x\n", code);
		return (LPTSTR)&tmp[0];
	}
}

void CCapture::combine_rectangle(byte* primary_buffer, byte* second_buffer, int line_stride, LPRECT rect, LPSIZE screen,
	int bitcount, int SMALL_WIDTH, int SMALL_HEIGHT, HRGN& region)
{
	int x, y;
	int num_bit = (bitcount >> 3);
	byte* dst_buf = primary_buffer;
	byte* bak_buf = second_buffer;

	for (y = max(rect->top, 0); y < rect->bottom; y += SMALL_HEIGHT) {
		int h = min(SMALL_HEIGHT, rect->bottom - y);
		for (x = max(rect->left, 0); x < rect->right; x += SMALL_WIDTH) {
			int w = min(SMALL_WIDTH, rect->right - x);
			int x2 = x + w;
			int y2 = y + h;
			if (x < 0) { x = 0; }
			else if (x >= screen->cx) { x = screen->cx; }
			if (y < 0) { y = 0; }
			else if (y >= screen->cy) { y = screen->cy; }
			if (x2 >= screen->cx) x2 = screen->cx; if (x2 <= x)continue;
			if (y2 >= screen->cy) y2 = screen->cy; if (y2 <= y)continue;

			w = x2 - x;
			h = y2 - y;
			int base_pos = line_stride*y + x*num_bit;
			int line_bytes = w*num_bit;

			int i;
			bool is_dirty = false;
#define    REP_CD(II) \
			if( !is_dirty){ \
				for (i = II; i < h; i += 5) { /*隔行扫描*/ \
					int pos = base_pos + line_stride*i;\
					if (memcmp(dst_buf + pos, bak_buf + pos, line_bytes) != 0) { is_dirty = true; break; }\
				}\
			}
			REP_CD(0); REP_CD(1);
			REP_CD(2); REP_CD(3); REP_CD(4);

			if (is_dirty) {
				if (!region) {
					region = CreateRectRgn(x, y, x2, y2);
				}
				else {
					HRGN n_rgn = CreateRectRgn(x, y, x2, y2);
					if (n_rgn) {
						if (CombineRgn(region, region, n_rgn, RGN_OR) == NULLREGION) { //矩形框可能有重叠，因此首先组成HRGN区域，然后再计算出合并之后的矩形框
							fprintf(m_fLog, "CombineRgn [%d, %d, %d, %d} err=%d\n", x, y, x2, y2, GetLastError());
							DeleteObject(region);
							region = NULL;
						}
						DeleteObject(n_rgn);
					}
				}
			}
		}
	}
}

void CCapture::region_to_rectangle(HRGN region, byte* dst_buf, int line_stride,
	int cx, int cy, int bitcount, ChangedRects** p_rc_array, int* p_rc_count)
{
	ChangedRects* rc = NULL;
	DWORD idx = 0;
	int num_bit = (bitcount >> 3);

	DWORD size = GetRegionData(region, NULL, NULL);
	tbuf_check(&m_tArr[0], size);
	LPRGNDATA rgnData = (LPRGNDATA)m_tArr[0].buf;
	if (GetRegionData(region, size, rgnData)) {
		DWORD cnt = rgnData->rdh.nCount;
		DWORD sz = cnt * sizeof(ChangedRects);
		tbuf_check(&m_tArr[1], sz);
		rc = (ChangedRects*)m_tArr[1].buf;
		LPRECT src_rc = (LPRECT)rgnData->Buffer;
		unsigned int i;
		for (i = 0; i < cnt; ++i) {
			ChangedRects* c = &rc[idx];;
			//裁剪矩形框，保证在可视区域
			int x = src_rc[i].left;
			if (x < 0) { x = 0; }
			else if (x >= cx) { x = cx; }
			int y = src_rc[i].top;
			if (y < 0) { y = 0; }
			else if (y >= cy) { y = cy; }
			int x2 = src_rc[i].right;
			if (x2 >= cx) x2 = cx;
			if (x2 <= x) continue;
			int y2 = src_rc[i].bottom;
			if (y2 >= cy) y2 = cy;
			if (y2 <= y) continue;
			c->left = x;
			c->top = y;
			c->right = x2;
			c->bottom = y2;
			c->buffer = (char*)dst_buf + line_stride * y + x * num_bit;
			++idx;
		}
	}
	*p_rc_array = rc;
	*p_rc_count = idx;
}

void CCapture::capture_mirror()
{
#ifdef DMF_MIRROR
	if (!m_Mirror.buffer.Userbuffer && abs(time(0) - m_Mirror.last_check_drawbuf_time) > 5) {
		m_Mirror.last_check_drawbuf_time = time(0);
		map_and_unmap_buffer(m_Mirror.disp.DeviceName, TRUE, &m_Mirror.buffer);
		//printf("map mirror buffer\n");
	}
	if (!m_Mirror.buffer.Userbuffer) {
		return;
	}

	//printf("mirror buffer counter %d\n", m_Mirror.buffer.buffer->counter);
	if (m_Mirror.buffer.buffer->counter != m_Mirror.index || m_ForceFrame) {
		m_Mirror.index = m_Mirror.buffer.buffer->counter;
		m_ForceFrame = FALSE;

		CallbackFrameInfo frm;
		frm.width = GetSystemMetrics(SM_CXSCREEN);
		frm.height = GetSystemMetrics(SM_CYSCREEN);
		frm.line_bytes = frm.width * 4;
		frm.line_stride = frm.width * 4;
		frm.bitcount = 32;
		frm.buffer = (char*)m_Mirror.buffer.Userbuffer;
		frm.length = frm.width * frm.height * 4;
		onFrame(&frm, onframe_param);
		m_CaptureSeq++;
	}
#else
	if (!m_Mirror.buffer && abs(time(0) - m_Mirror.last_check_drawbuf_time) > 5) {
		m_Mirror.last_check_drawbuf_time = time(0);
		map_and_unmap_buffer(m_Mirror.disp.DeviceName, TRUE, &m_Mirror.buffer);
	}
	if (!m_Mirror.buffer) {
		return;
	}

	CallbackFrameInfo frm;
	frm.width = m_Mirror.buffer->cx;
	frm.height = m_Mirror.buffer->cy;
	frm.line_bytes = m_Mirror.buffer->line_bytes;
	frm.line_stride = m_Mirror.buffer->line_stride;
	frm.bitcount = m_Mirror.buffer->bitcount;
	frm.buffer = (char*)m_Mirror.buffer->data_buffer;
	frm.length = m_Mirror.buffer->data_length;
	onFrame(&frm, onframe_param);
	m_CaptureSeq++;
#endif
}

void CCapture::capture_dxgi()
{
	if (!m_Directx.dxgi_dup) {
		return;
	}

	HRESULT hr;
	IDXGIResource* DesktopResource = NULL;
	DXGI_OUTDUPL_FRAME_INFO FrameInfo;
	ID3D11Texture2D* image2d = NULL;

	if (m_Directx.is_acquire_frame) {
		m_Directx.dxgi_dup->ReleaseFrame();
		m_Directx.is_acquire_frame = 0;
	}
	hr = m_Directx.dxgi_dup->AcquireNextFrame(0, &FrameInfo, &DesktopResource);
	if (FAILED(hr)) {   
		if (hr == _HRESULT_TYPEDEF_(0x887A0026L) || hr == _HRESULT_TYPEDEF_(0x887A0001L)) {
			fprintf(m_fLog, "directx reset\n");
			init_directx(FALSE);
			init_directx(TRUE);
			return;
		}
		if (!m_Directx.buffer)
			return;
		//printf("acquire frame fail 0x%X\n", hr);
		goto L;
	}
	m_Directx.is_acquire_frame = 1;

	SAFE_RELEASE(image2d);
	hr = DesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&image2d);
	SAFE_RELEASE(DesktopResource);
	if (FAILED(hr)) {
		fprintf(m_fLog, "Query Texture2D err=0x%X\n", hr);
		return;
	}

	DXGI_MAPPED_RECT mappedRect;
	if (!m_Directx.dxgi_text2d) {
		D3D11_TEXTURE2D_DESC frameDescriptor;
		image2d->GetDesc(&frameDescriptor);
		frameDescriptor.Usage = D3D11_USAGE_STAGING;
		frameDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		frameDescriptor.BindFlags = 0;
		frameDescriptor.MiscFlags = 0;
		frameDescriptor.MipLevels = 1;
		frameDescriptor.ArraySize = 1;
		frameDescriptor.SampleDesc.Count = 1;
		hr = m_Directx.d11dev->CreateTexture2D(&frameDescriptor, NULL, &m_Directx.dxgi_text2d);
		if (FAILED(hr)) {
			fprintf(m_fLog, "CreateTexture2D err=0x%X\n", hr);
			return;
		}
		m_Directx.dxgi_text2d->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);
		hr = m_Directx.dxgi_text2d->QueryInterface(__uuidof(IDXGISurface), (void**)&m_Directx.dxgi_surf);
		if (FAILED(hr)) {
			SAFE_RELEASE(m_Directx.dxgi_text2d);
			fprintf(m_fLog, "QueryInterface dxgi_surf err=0x%X\n", hr);
			return;
		}
		hr = m_Directx.dxgi_surf->Map(&mappedRect, DXGI_MAP_READ);
		if (FAILED(hr)) {
			fprintf(m_fLog, "Map Data buffer error\n");
			SAFE_RELEASE(m_Directx.dxgi_surf);
			SAFE_RELEASE(m_Directx.dxgi_text2d);
			return;
		}

		if (!m_Directx.buffer) {
			m_Directx.buffer = (byte*)mappedRect.pBits;
		}
		if (!m_Directx.bak_buffer) {
			m_Directx.bak_buffer = (byte*)malloc(m_Directx.line_stride * m_Directx.cy);
		}
	}
	m_Directx.d11ctx->CopyResource(m_Directx.dxgi_text2d, image2d);

L:
	if (m_UseCamera) {
		if (FrameInfo.TotalMetadataBufferSize > 0) {
			memcpy(m_Directx.bak_buffer, m_Directx.buffer, m_Directx.line_stride * m_Directx.cy);
		}
		if (isCaptureDone(m_CameraID)) {
			//fprintf(m_fLog, "Camera capture %d-%d, Screen capture %d-%d, seq %d\n", m_CamCapture.mWidth, m_CamCapture.mHeight, m_Directx.line_stride, m_Directx.cy, m_CaptureSeq);
			int i = 0;
			int j = 0;
			int k = 0;
			int iStart = (m_Directx.cy - m_CamCapture.mHeight) * m_Directx.line_stride;
			int jStart = 0;
			char* pData = (char*)m_CamCapture.mTargetBuf;
			for (k = 0; k < m_CamCapture.mHeight; k++) {
				for (i = iStart; i < iStart + m_Directx.line_stride && j < jStart + m_CamCapture.mWidth * 4; i++, j++) {
					m_Directx.bak_buffer[i] = pData[j];
				}
				iStart += m_Directx.line_stride;
				jStart += m_CamCapture.mWidth * 4;
			}
			CallbackFrameInfo frm;
			frm.width = m_Directx.cx;
			frm.height = m_Directx.cy;
			frm.line_bytes = m_Directx.line_bytes;
			frm.line_stride = m_Directx.line_stride;
			frm.bitcount = m_Directx.bitcount;
			frm.buffer = (char*)m_Directx.bak_buffer;
			frm.length = m_Directx.line_stride * m_Directx.cy;
			onFrame(&frm, onframe_param);
			m_CaptureSeq++;
			doCapture(m_CameraID);
		}
	}
	else {
		if (FrameInfo.TotalMetadataBufferSize > 0 || m_ForceFrame) {
			m_ForceFrame = FALSE;
			CallbackFrameInfo frm;
			frm.width = m_Directx.cx;
			frm.height = m_Directx.cy;
			frm.line_bytes = m_Directx.line_bytes;
			frm.line_stride = m_Directx.line_stride;
			frm.bitcount = m_Directx.bitcount;
			frm.buffer = (char*)m_Directx.buffer;
			frm.length = m_Directx.line_stride * m_Directx.cy;
			onFrame(&frm, onframe_param);
			m_CaptureSeq++;
		}
	}
}

void CCapture::capture_gdi()
{
	if (!m_GDI.buffer || !m_GDI.hbmp)return;

	HDC hdc = CreateDC(NULL, m_DispName, NULL, NULL);
	BOOL ret = BitBlt(m_GDI.memdc, 0, 0, m_GDI.cx, m_GDI.cy, hdc, 0, 0, SRCCOPY | CAPTUREBLT);
	if (!ret) {
		fprintf(m_fLog, "gdi bitblt fail %lu\n", GetLastError());
		return;
	}
	ReleaseDC(NULL, hdc);

	if (m_UseCamera) {
		if (isCaptureDone(m_CameraID)) {
			int i = 0;
			int j = 0;
			int k = 0;
			int iStart = (m_GDI.cy - m_CamCapture.mHeight) * m_GDI.line_stride;
			int jStart = 0;
			char* pData = (char*)m_CamCapture.mTargetBuf;
			for (k = 0; k < m_CamCapture.mHeight; k++) {
				for (i = iStart; i < iStart + m_GDI.line_stride && j < jStart + m_CamCapture.mWidth * 4; i++, j++) {
					m_GDI.buffer[i] = pData[j];
				}
				iStart += m_GDI.line_stride;
				jStart += m_CamCapture.mWidth * 4;
			}
			CallbackFrameInfo frm;
			frm.width = m_GDI.cx;
			frm.height = m_GDI.cy;
			frm.line_bytes = m_GDI.line_bytes;
			frm.line_stride = m_GDI.line_stride;
			frm.bitcount = m_GDI.bitcount;
			frm.buffer = (char*)m_GDI.buffer;
			frm.length = m_GDI.line_stride * m_GDI.cy;
			onFrame(&frm, onframe_param);
			m_CaptureSeq++;
			doCapture(m_CameraID);
		}	
	}
	else {
		HRGN region = NULL;
		SIZE screenSize;
		screenSize.cx = m_GDI.cx;
		screenSize.cy = m_GDI.cy;
		RECT screeRc = { 0, 0, m_GDI.cx, m_GDI.cy };
		combine_rectangle(m_GDI.buffer, m_GDI.back_buf, m_GDI.line_stride, &screeRc, &screenSize, m_GDI.bitcount,
			GDI_SMALL_RECT_WIDTH, GDI_SMALL_RECT_HEIGHT, region);

		int rc_count = 0;
		ChangedRects* rc_array = NULL;
		if (region) {
			region_to_rectangle(region, m_GDI.buffer, m_GDI.line_stride, m_GDI.cx, m_GDI.cy,
				m_GDI.bitcount, &rc_array, &rc_count);
			DeleteObject(region);
		}

		if (rc_count > 0 || m_ForceFrame) {
			m_ForceFrame = FALSE;
			memcpy(m_GDI.back_buf, m_GDI.buffer, m_GDI.line_stride * m_GDI.cy);
			CallbackFrameInfo frm;
			frm.width = m_GDI.cx;
			frm.height = m_GDI.cy;
			frm.line_bytes = m_GDI.line_bytes;
			frm.line_stride = m_GDI.line_stride;
			frm.bitcount = m_GDI.bitcount;
			frm.buffer = (char*)m_GDI.buffer;
			frm.length = m_GDI.line_stride * m_GDI.cy;
			onFrame(&frm, onframe_param);
			m_CaptureSeq++;
		}
	}
}

void CCapture::capture_camera()
{
	if (isCaptureDone(m_CameraID)) {
		CallbackFrameInfo frm;
		frm.width = m_CamCapture.mWidth;
		frm.height = m_CamCapture.mHeight;
		frm.line_bytes = m_CamCapture.mWidth * 4;
		frm.line_stride = m_CamCapture.mWidth * 4;
		frm.bitcount = 32;
		frm.buffer = (char*)m_CamCapture.mTargetBuf;
		frm.length = m_CamCapture.mWidth * m_CamCapture.mHeight * 4;
		onFrame(&frm, onframe_param);
		m_CaptureSeq++;
		doCapture(m_CameraID);
	}
}

BOOL CCapture::init_mirror(BOOL is_init)
{
	BOOL r = FALSE;
	if (is_init) {
#ifdef DMF_MIRROR
		m_Mirror.buffer.Userbuffer = NULL;
#else
		m_Mirror.buffer = NULL;
#endif
		m_Mirror.is_active = FALSE;
		r = active_mirror_driver(TRUE, &m_Mirror.disp);
		if (!r) {
			fprintf(m_fLog, "Active Mirror Driver Error\n");
		}
		else {
			m_Mirror.is_active = TRUE;
		}
		m_Mirror.index = -1;
		return m_Mirror.is_active;
	}
	else { // deinit
#ifdef DMF_MIRROR
		if (m_Mirror.buffer.Userbuffer) {
			m_Mirror.buffer.Userbuffer = NULL;
#else
		if (m_Mirror.buffer) {
			m_Mirror.buffer = NULL;
#endif
			map_and_unmap_buffer(m_Mirror.disp.DeviceName, FALSE, &m_Mirror.buffer);
		}

		m_Mirror.is_active = FALSE;
		m_Mirror.index = -1;
		r = active_mirror_driver(FALSE, &m_Mirror.disp);
		fprintf(m_fLog, "unload mirror driver ret=%d\n", r);
		return r;
	}
}

BOOL CCapture::init_dxgi()
{
	HRESULT hr;
	static HMODULE hmod = 0; 
	if (!hmod)hmod = LoadLibrary("d3d11.dll");
	fnD3D11CreateDevice fn = (fnD3D11CreateDevice)GetProcAddress(hmod, "D3D11CreateDevice");
	if (!fn) {
		fprintf(m_fLog, "DXGI init :Not Load [D3D11CreateDevice] function \n");
		return FALSE;
	}
	D3D_DRIVER_TYPE DriverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT NumDriverTypes = ARRAYSIZE(DriverTypes);
	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_1
	};
	UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);
	D3D_FEATURE_LEVEL FeatureLevel;
	for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex) {
		hr = fn(nullptr, DriverTypes[DriverTypeIndex], nullptr, 0, FeatureLevels, NumFeatureLevels,
			D3D11_SDK_VERSION, &m_Directx.d11dev, &FeatureLevel, &m_Directx.d11ctx);
		if (SUCCEEDED(hr))break;
	}
	if (FAILED(hr)) {
		fprintf(m_fLog, "DXGI Init: D3D11CreateDevice Return hr=0x%X\n", hr);
		return FALSE;
	}
	IDXGIDevice* dxdev = 0;
	hr = m_Directx.d11dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxdev);
	if (FAILED(hr)) {
		init_directx(FALSE); 
		fprintf(m_fLog, "DXGI init: IDXGIDevice Error hr=0x%X\n", hr);
		return FALSE;
	}
	IDXGIAdapter* DxgiAdapter = 0;
	hr = dxdev->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&DxgiAdapter));
	SAFE_RELEASE(dxdev);
	if (FAILED(hr)) {
		init_directx(FALSE); 
		fprintf(m_fLog, "DXGI init: IDXGIAdapter Error hr=0x%X\n", hr);
		return FALSE;
	}
	INT nOutput = m_MonitorID;
	IDXGIOutput* DxgiOutput = 0;
	hr = DxgiAdapter->EnumOutputs(nOutput, &DxgiOutput);
	SAFE_RELEASE(DxgiAdapter);
	if (FAILED(hr)) {
		init_directx(FALSE);
		fprintf(m_fLog, "DXGI init: IDXGIOutput Error hr=0x%X\n", hr);
		return FALSE;
	}
	DxgiOutput->GetDesc(&m_Directx.dxgi_desc);
	IDXGIOutput1* DxgiOutput1 = 0;
	hr = DxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&DxgiOutput1));
	SAFE_RELEASE(DxgiOutput);
	if (FAILED(hr)) {
		init_directx(FALSE); 
		fprintf(m_fLog, "DXGI init: IDXGIOutput1 Error hr=0x%X\n", hr);
		return FALSE;
	}
	hr = DxgiOutput1->DuplicateOutput(m_Directx.d11dev, &m_Directx.dxgi_dup);
	SAFE_RELEASE(DxgiOutput1);
	if (FAILED(hr)) {
		init_directx(FALSE);
		fprintf(m_fLog, "DXGI init: IDXGIOutputDuplication Error hr=0x%X\n", hr);
		return FALSE;
	}
	return TRUE;
}

BOOL CCapture::init_directx(BOOL is_init)
{
	if (is_init) {
		DISPLAY_DEVICE dispDevice;
		DEVMODE devmode;
		memset(&devmode, 0, sizeof(DEVMODE));
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmDriverExtra = 0;
		BOOL bRet = find_display_device(&dispDevice, &devmode, FALSE, m_MonitorID);
		if (!bRet) {
			m_MonitorID = 0;
			bRet = find_display_device(&dispDevice, &devmode, FALSE, 0);
			if (!bRet) return FALSE;
		}

		m_Directx.cx = devmode.dmPelsWidth;
		m_Directx.cy = devmode.dmPelsHeight;
		m_Directx.bitcount = devmode.dmBitsPerPel;
		if (m_Directx.bitcount != 8 && m_Directx.bitcount != 16 && m_Directx.bitcount != 32 && m_Directx.bitcount != 24) {
			return FALSE;
		}
		m_Directx.line_bytes = m_Directx.cx * m_Directx.bitcount / 8;
		m_Directx.line_stride = (m_Directx.line_bytes + 3) / 4 * 4;

		//DXGI above win8
		if (init_dxgi()) {
			return TRUE;
		}

		return FALSE;
	}
	else {
		SAFE_RELEASE(m_Directx.dxgi_dup);
		SAFE_RELEASE(m_Directx.d11dev);
		SAFE_RELEASE(m_Directx.d11ctx);
		SAFE_RELEASE(m_Directx.dxgi_text2d);
		if (m_Directx.dxgi_surf) {
			m_Directx.dxgi_surf->Unmap();
		}
		SAFE_RELEASE(m_Directx.dxgi_surf);
		if (m_Directx.buffer) {
			m_Directx.buffer = NULL;
		}
		SAFE_FREE(m_Directx.bak_buffer);
		m_Directx.is_acquire_frame = 0;
		//Sleep(2000);

		return TRUE;
	}

	return FALSE;
}

BOOL CCapture::init_gdi(BOOL is_init)
{
	if (is_init) {
		DISPLAY_DEVICE dispDevice;
		DEVMODE devmode;
		memset(&devmode, 0, sizeof(DEVMODE));
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmDriverExtra = 0;
		BOOL bRet = find_display_device(&dispDevice, &devmode, FALSE, m_MonitorID);
		if (!bRet) {
			m_MonitorID = 0;
			bRet = find_display_device(&dispDevice, &devmode, FALSE, 0);
			if (!bRet) return FALSE;
		}

		m_GDI.cx = devmode.dmPelsWidth;
		m_GDI.cy = devmode.dmPelsHeight;
		m_GDI.bitcount = devmode.dmBitsPerPel;
		if (m_GDI.bitcount != 8 && m_GDI.bitcount != 16 && m_GDI.bitcount != 32 && m_GDI.bitcount != 24) {
			return FALSE;
		}
		m_GDI.line_bytes = m_GDI.cx * m_GDI.bitcount / 8;
		m_GDI.line_stride = (m_GDI.line_bytes + 3) / 4 * 4;

		BITMAPINFOHEADER bi; memset(&bi, 0, sizeof(bi));
		bi.biSize = sizeof(bi);
		bi.biWidth = m_GDI.cx;
		bi.biHeight = -m_GDI.cy; //从上朝下扫描
		bi.biPlanes = 1;
		bi.biBitCount = m_GDI.bitcount; //RGB
		bi.biCompression = BI_RGB;
		bi.biSizeImage = 0;
		BYTE bb[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256];
		memcpy(bb, &bi, sizeof(bi));
		HDC hdc = CreateDC(NULL, m_DispName, NULL, NULL); //屏幕DC
		if (m_GDI.bitcount == 8) {//系统调色板
			PALETTEENTRY pal[256]; memset(pal, 0, sizeof(pal));
			GetSystemPaletteEntries(hdc, 0, 256, pal); //获取系统调色板
			RGBQUAD* PalColor = (RGBQUAD*)(bb + sizeof(bi));
			for (int i = 0; i < 256; ++i) {
				PalColor[i].rgbReserved = 0;
				PalColor[i].rgbRed = pal[i].peRed;
				PalColor[i].rgbGreen = pal[i].peGreen;
				PalColor[i].rgbBlue = pal[i].peBlue;
			}
		}

		m_GDI.memdc = CreateCompatibleDC(hdc);
		m_GDI.buffer = NULL;
		m_GDI.hbmp = CreateDIBSection(hdc, (BITMAPINFO*)bb, DIB_RGB_COLORS, (void**)&m_GDI.buffer, NULL, 0);
		ReleaseDC(NULL, hdc);
		if (!m_GDI.buffer) {
			DeleteDC(m_GDI.memdc); m_GDI.memdc = 0;
			return FALSE;
		}
		SelectObject(m_GDI.memdc, m_GDI.hbmp);
		m_GDI.back_buf = (byte*)malloc(m_GDI.line_stride * m_GDI.cy);
		return TRUE;
	}
	else {
		m_GDI.buffer = NULL;
		DeleteDC(m_GDI.memdc); m_GDI.memdc = NULL;
		DeleteObject(m_GDI.hbmp); m_GDI.hbmp = NULL;
		if (m_GDI.back_buf) {
			free(m_GDI.back_buf);
			m_GDI.back_buf = NULL;
		}
	}

	return FALSE;
}


BOOL CCapture::init_camera(BOOL is_init)
{
	if (is_init) {
		int count = countCaptureDevices();
		if (count == 0) {
			fprintf(m_fLog, "ESCAPI initialization failure or no devices found\n");
			return FALSE;
		}
		
		m_CamCapture.mWidth = 640;
		m_CamCapture.mHeight = 480;
		m_CamCapture.mTargetBuf = new int[m_CamCapture.mWidth * m_CamCapture.mHeight];
		if (initCapture(m_CameraID, &m_CamCapture) == 0) {
			fprintf(m_fLog, "Capture failed - device may already be in use\n");
			return FALSE;
		}
	}
	else {
		deinitCapture(m_CameraID);
	}
	return TRUE;
}