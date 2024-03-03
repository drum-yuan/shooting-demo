#include <stdio.h>
#include <time.h>
#include <string.h>
#include "Video.h"
#include "libyuv.h"
#include "util.h"

static void svc_encoder_log(void* context, int level, const char* message)
{
	LOG_DEBUG(message);
}

typedef struct DXVA2DevicePriv {
	HMODULE d3dlib;
	HMODULE dxva2lib;

	HANDLE device_handle;

	IDirect3D9       *d3d9;
	IDirect3DDevice9 *d3d9device;
} DXVA2DevicePriv;

Video::Video():m_iFrameW(0),
				m_iFrameH(0),
				m_iFrameRate(25),
				m_iFrameInterval(30),
				m_Bitrate(ENCODER_BITRATE),
				m_bPublisher(false),
				m_bForceKeyframe(false),
				m_bShow(true)
{
	m_pSVCEncoder = NULL;
	onEncoded = NULL;
	m_pYUVData = NULL;
	m_pRawFile = NULL;

	m_hRenderWin = NULL;
	m_pAVDecoder = NULL;
	m_pAVDecoderContext = NULL;
	m_Hwctx = NULL;
	m_AVVideoFrame = NULL;
	m_HwVideoFrame = NULL;
	m_HwPixFmt = AV_PIX_FMT_NONE;
	InitializeCriticalSection(&m_Cs);
	OpenDecoder();
	cap_initialize(Video::onFrame, this);
}

Video::~Video()
{
	cap_uninitialize();
	CloseEncoder();
	CloseDecoder();
}

void Video::set_render_win(void* hWnd)
{
	m_hRenderWin = hWnd;
}

bool Video::show(unsigned char* buffer, unsigned int len, bool is_show)
{
	int status = 0;

	LOG_INFO("show buffer len %u-%d", len, is_show);
	av_init_packet(&m_AVPacket);
	m_AVPacket.data = (uint8_t*)buffer;
	m_AVPacket.size = len;
	status = avcodec_send_packet(m_pAVDecoderContext, &m_AVPacket);
	if (status < 0) {
		LOG_ERROR("avcodec send packet failed! %d", status);
		return false;
	}
	m_AVVideoFrame->format = AV_PIX_FMT_NV12;
	do {
		status = avcodec_receive_frame(m_pAVDecoderContext, m_Hwctx? m_HwVideoFrame : m_AVVideoFrame);
	} while (status == AVERROR(EAGAIN));
	if (status < 0) {
		LOG_ERROR("avcodec receive frame failed! %d", status);
		return false;
	}

	if (is_show && m_bShow) {
		DXVA2Render();
	}
	return true;
}

void Video::set_on_encoded(onEncode_fp fp)
{
	LOG_DEBUG("set onEncoded");
	onEncoded = fp;
}

void Video::start()
{
	LOG_DEBUG("video start");
	m_bPublisher = true;

	char file_path[256];
	SYSTEMTIME t;
	GetLocalTime(&t);
	sprintf(file_path, "video/raw-%04d%02d%02d%02d%02d%02d.h264", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	m_pRawFile = fopen(file_path, "wb");
	m_sFileName = string(file_path);

	cap_start_capture_screen(0, true);
	cap_set_drop_interval(m_iFrameInterval);
	cap_set_frame_rate(m_iFrameRate);
}

void Video::stop()
{
	LOG_DEBUG("video stop");
	m_bPublisher = false;

	cap_stop_capture_screen();

	m_sFileName.clear();
	fflush(m_pRawFile);
	fclose(m_pRawFile);
}

void Video::pause()
{
	cap_pause_capture();
	m_bShow = false;
}

void Video::resume()
{
	cap_resume_capture();
	m_bShow = true;
}

void Video::reset_keyframe(bool reset_ack)
{
	m_bForceKeyframe = true;
	if (reset_ack) {
		cap_reset_sequence();
	}
}

void Video::write_data(unsigned char* data, int len)
{
	if (m_pRawFile) {
		fwrite(data, 1, len, m_pRawFile);
	}
}

string Video::get_raw_file_name()
{
	return m_sFileName;
}

void Video::onFrame(CallbackFrameInfo* frame, void* param)
{
	Video* video = (Video*)param;

	if (video->m_iFrameW != frame->width || video->m_iFrameH != frame->height || video->m_pYUVData == NULL) {
		LOG_INFO("onFrame change size: seq %d w %d h %d", cap_get_capture_sequence(), frame->width, frame->height);
		video->CloseEncoder();
		if (video->m_pYUVData)
			delete[] video->m_pYUVData;
		video->m_pYUVData = new unsigned char[frame->width * frame->height * 3 / 2];
		video->OpenEncoder(frame->width, frame->height);
		cap_reset_sequence();
	}

	if (frame->bitcount == 8) {
		LOG_INFO("capture 8bit");
		return;
	}
	else if (frame->bitcount == 16) {
		LOG_INFO("capture 16bit");
	}
	else if (frame->bitcount == 24) {
		//libyuv::RGB24ToI420();
		LOG_INFO("capture 24bit");
	}
	else if (frame->bitcount == 32) {
		//LOG_INFO("capture 32bit len %d", frame->length);
		int uv_stride = video->m_iFrameW / 2;
		uint8_t* y = video->m_pYUVData;
		uint8_t* u = video->m_pYUVData + video->m_iFrameW * video->m_iFrameH;
		uint8_t* v = video->m_pYUVData + video->m_iFrameW * video->m_iFrameH + video->m_iFrameH / 2 * uv_stride;
		libyuv::ARGBToI420((uint8_t*)frame->buffer, frame->line_stride, y, video->m_iFrameW, u, uv_stride, v, uv_stride, video->m_iFrameW, video->m_iFrameH);
		/*static int cnt = 0;
		if (cnt < 10) {
		FILE* fp = fopen("yuv_data.yuv", "ab");
		fwrite(video->m_pYUVData, 1, video->m_iFrameW * video->m_iFrameH * 3 / 2, fp);
		fclose(fp);
		cnt++;
		}*/
	}
	video->Encode();
}

bool Video::OpenEncoder(int w, int h)
{
	m_iFrameW = w;
	m_iFrameH = h;
	LOG_INFO("OpenEncoder w=%d h=%d", m_iFrameW, m_iFrameH);
	if (!m_pSVCEncoder)
	{
		if (WelsCreateSVCEncoder(&m_pSVCEncoder) || (NULL == m_pSVCEncoder))
		{
			LOG_ERROR("Create encoder failed, this = %p", this);
			return false;
		}

		WelsTraceCallback pFunc = svc_encoder_log;
		m_pSVCEncoder->SetOption(ENCODER_OPTION_TRACE_CALLBACK_CONTEXT, NULL);
		m_pSVCEncoder->SetOption(ENCODER_OPTION_TRACE_CALLBACK, &pFunc);
		SEncParamExt tSvcParam;
		m_pSVCEncoder->GetDefaultParams(&tSvcParam);
		FillSpecificParameters(tSvcParam);		
		int nRet = m_pSVCEncoder->InitializeExt(&tSvcParam);
		if (nRet != 0)
		{
			LOG_ERROR("init encoder failed, this = %p, error = %d", this, nRet);
			return false;
		}
	}
	return true;
}

void Video::CloseEncoder()
{
	if (m_pSVCEncoder)
	{
		WelsDestroySVCEncoder(m_pSVCEncoder);
		m_pSVCEncoder = NULL;
	}
}

void Video::FillSpecificParameters(SEncParamExt &sParam)
{
	sParam.iUsageType = CAMERA_VIDEO_REAL_TIME;
	sParam.fMaxFrameRate = 30;    // input frame rate
	sParam.iPicWidth = m_iFrameW;         // width of picture in samples
	sParam.iPicHeight = m_iFrameH;         // height of picture in samples
	sParam.iTargetBitrate = m_Bitrate; // target bitrate desired
	sParam.iMaxBitrate = UNSPECIFIED_BIT_RATE;
	sParam.iRCMode = RC_BITRATE_MODE;      //  rc mode control
	sParam.iTemporalLayerNum = 1;          // layer number at temporal level
	sParam.iSpatialLayerNum = 1;           // layer number at spatial level
	sParam.iEntropyCodingModeFlag = 0; 	//#0:cavlc 1:cabac;
	sParam.bEnableDenoise = 0;             // denoise control
	sParam.bEnableBackgroundDetection = 0; // background detection control
	sParam.bEnableAdaptiveQuant = 0;       // adaptive quantization control
	sParam.bEnableFrameSkip = 1;           // frame skipping
	sParam.bEnableLongTermReference = 0;   // long term reference control
	sParam.iLtrMarkPeriod = 30;
	//sParam.uiIntraPeriod = 320; // period of Intra frame
	sParam.eSpsPpsIdStrategy = INCREASING_ID;
	sParam.bPrefixNalAddingCtrl = 0;
	sParam.iComplexityMode = LOW_COMPLEXITY;
	sParam.bSimulcastAVC = false;
	sParam.iMaxQp = 48;
	sParam.iMinQp = 24;
	sParam.uiMaxNalSize = 0;

	//layer cfg
	SSpatialLayerConfig *pDLayer = &sParam.sSpatialLayers[0];
	/*pDLayer->iVideoWidth = m_iFrameW;
	pDLayer->iVideoHeight = m_iFrameH;
	pDLayer->fFrameRate = (float)m_iFrameRate;
	pDLayer->iSpatialBitrate = m_Bitrate / 8;
	pDLayer->iMaxSpatialBitrate = UNSPECIFIED_BIT_RATE;

	pDLayer = &sParam.sSpatialLayers[1];*/
	pDLayer->iVideoWidth = m_iFrameW;
	pDLayer->iVideoHeight = m_iFrameH;
	pDLayer->fFrameRate = (float)m_iFrameRate;
	pDLayer->iSpatialBitrate = m_Bitrate / 2;
	pDLayer->iMaxSpatialBitrate = UNSPECIFIED_BIT_RATE;
}

void Video::Encode()
{
	if (!m_pSVCEncoder) {
		LOG_DEBUG("SVCEncoder null");
		return;
	}

	if (m_bForceKeyframe) {
		m_pSVCEncoder->ForceIntraFrame(true);
	}

	SSourcePicture tSrcPic;
	unsigned int uTimeStamp = clock();
	tSrcPic.uiTimeStamp = uTimeStamp;
	tSrcPic.iColorFormat = videoFormatI420;
	tSrcPic.iStride[0] = m_iFrameW;
	tSrcPic.iStride[1] = tSrcPic.iStride[0] >> 1;
	tSrcPic.iStride[2] = tSrcPic.iStride[1];
	tSrcPic.iPicWidth = m_iFrameW;
	tSrcPic.iPicHeight = m_iFrameH;

	tSrcPic.pData[0] = m_pYUVData;
	tSrcPic.pData[1] = tSrcPic.pData[0] + (m_iFrameW * m_iFrameH);
	tSrcPic.pData[2] = tSrcPic.pData[1] + (m_iFrameW * m_iFrameH / 4);

	SFrameBSInfo tFbi;
	int iEncFrames = m_pSVCEncoder->EncodeFrame(&tSrcPic, &tFbi);
	if (iEncFrames == cmResultSuccess)
	{
		tFbi.uiTimeStamp = uTimeStamp;
		if (onEncoded) {
			onEncoded(&tFbi);
			m_bForceKeyframe = false;
		}
	}
	else {
		LOG_ERROR("encodeVideo failed");
	}
}

bool Video::OpenDecoder()
{
	m_pAVDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (m_pAVDecoder) {
		m_pAVDecoderContext = avcodec_alloc_context3(m_pAVDecoder);
	}
	if (m_pAVDecoderContext == NULL) {
		LOG_ERROR("alloc context failed");
		return false;
	}
	if (m_pAVDecoder->capabilities & AV_CODEC_CAP_TRUNCATED) {
		m_pAVDecoderContext->flags |= AV_CODEC_FLAG_TRUNCATED;
	}
	if (av_hwdevice_ctx_create(&m_Hwctx, AV_HWDEVICE_TYPE_DXVA2, NULL, NULL, 0) < 0) {
		avcodec_free_context(&m_pAVDecoderContext);
		LOG_ERROR("create hwdevice ctx failed");
		return false;
	}
	m_pAVDecoderContext->hw_device_ctx = av_buffer_ref(m_Hwctx);
	m_pAVDecoderContext->get_format = get_hw_format;
	m_HwPixFmt = AV_PIX_FMT_DXVA2_VLD;
	m_pAVDecoderContext->opaque = this;

	if (avcodec_open2(m_pAVDecoderContext, m_pAVDecoder, NULL) < 0) {
		avcodec_free_context(&m_pAVDecoderContext);
		LOG_ERROR("avcodec open failed");
		return false;
	}
	m_AVVideoFrame = av_frame_alloc();
	m_AVVideoFrame->pts = 0;
	m_HwVideoFrame = av_frame_alloc();
	return true;
}

void Video::CloseDecoder()
{
	if (m_HwVideoFrame) {
		av_frame_free(&m_HwVideoFrame);
	}
	if (m_Hwctx) {
		av_buffer_unref(&m_Hwctx);
	}
	if (m_pAVDecoderContext) {
		avcodec_close(m_pAVDecoderContext);
		avcodec_free_context(&m_pAVDecoderContext);
	}
}

void Video::DXVA2Render()
{
	if (m_hRenderWin == NULL) {
		return;
	}

	if (!m_Hwctx) {
		return;
	}
	IDirect3DSurface9* backBuffer = NULL;
	LPDIRECT3DSURFACE9 surface = (LPDIRECT3DSURFACE9)m_HwVideoFrame->data[3];
	AVHWDeviceContext* ctx = (AVHWDeviceContext*)m_Hwctx->data;
	DXVA2DevicePriv* priv = (DXVA2DevicePriv*)ctx->user_opaque;
	HRESULT ret = 0;

	m_iFrameW = m_HwVideoFrame->width;
	m_iFrameH = m_HwVideoFrame->height;

	EnterCriticalSection(&m_Cs);
	ret = priv->d3d9device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	if (ret < 0) {
		LOG_ERROR("Clear failed\n");
	}
	ret = priv->d3d9device->BeginScene();
	if (ret < 0) {
		LOG_ERROR("BeginScene failed\n");
	}
	ret = priv->d3d9device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
	if (ret < 0) {
		LOG_ERROR("GetBackBuffer failed\n");
	}
	ret = priv->d3d9device->StretchRect(surface, NULL, backBuffer, NULL, D3DTEXF_LINEAR);
	if (ret < 0) {
		LOG_ERROR("StretchRect failed\n");
	}
	ret = priv->d3d9device->EndScene();
	if (ret < 0) {
		LOG_ERROR("EndScene failed\n");
	}
	ret = priv->d3d9device->Present(NULL, NULL, (HWND)m_hRenderWin, NULL);
	if (ret < 0) {
		LOG_ERROR("Present failed\n");
	}
	LeaveCriticalSection(&m_Cs);
	backBuffer->Release();
}

enum AVPixelFormat Video::get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
	Video* pVideo = (Video*)ctx->opaque;
	const enum AVPixelFormat *p;
	for (p = pix_fmts; *p != -1; p++) {
		if (*p == pVideo->m_HwPixFmt)
			return *p;
	}

	LOG_ERROR("Failed to get HW surface format.");
	return AV_PIX_FMT_NONE;
}

bool Video::is_publisher()
{
	return m_bPublisher;
}

void Video::get_stream_size(int* width, int* height)
{
	if (m_iFrameW > 0 && m_iFrameH > 0) {
		*width = m_iFrameW;
		*height = m_iFrameH;
	}
}
