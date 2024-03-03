#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include <d3d9.h>
#include "CaptureApi.h"
#include "wels/codec_def.h"
#include "wels/codec_app_def.h"
#include "wels/codec_api.h"
#include <functional>
#include <thread>

#define MAX_FRAME_WIDTH		1920
#define MAX_FRAME_HEIGHT	1080

#define ENCODER_BITRATE		8000000

typedef std::function<void(void* data)> onEncode_fp;
class Video
{
public:
	Video();
	~Video();

	void set_render_win(void* hWnd);
	bool show(unsigned char* buffer, unsigned int len, bool is_show);

	void set_on_encoded(onEncode_fp fp);
	bool is_publisher();
	void get_stream_size(int* width, int* height);
	void start();
	void stop();
	void pause();
	void resume();
	void reset_keyframe(bool reset_ack);
	void write_data(unsigned char* data, int len);
	std::string get_raw_file_name();

	static void onFrame(CallbackFrameInfo* frame, void* param);
	static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

private:
	void FillSpecificParameters(SEncParamExt &sParam);
	bool OpenEncoder(int w, int h);
	void CloseEncoder();
	void Encode();
	bool OpenDecoder();
	void CloseDecoder();
	void DXVA2Render();

	ISVCEncoder* m_pSVCEncoder;
	onEncode_fp onEncoded;
	unsigned char* m_pYUVData;
	std::string m_sFileName;
	FILE* m_pRawFile;

	int m_iFrameW;
	int m_iFrameH;
	int m_iFrameRate;
	int m_iFrameInterval;
	int m_Bitrate;
	bool m_bPublisher;
	bool m_bForceKeyframe;
	bool m_bShow;

	void* m_hRenderWin;

	AVCodec* m_pAVDecoder;
	AVCodecContext* m_pAVDecoderContext;
	AVPacket m_AVPacket;
	AVBufferRef* m_Hwctx;
	AVFrame* m_AVVideoFrame;
	AVFrame* m_HwVideoFrame;
	enum AVPixelFormat m_HwPixFmt;
	CRITICAL_SECTION m_Cs;
};
