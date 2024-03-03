#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include "libwebsockets.h"
#include "video.h"
#include "buffer.h"

using namespace std;

typedef std::function<void(void)> StartStreamCallback;
typedef std::function<void(const std::string&)> StopStreamCallback;
typedef std::function<void(void)> QuitStreamCallback;

class SocketsClient
{
public:
	SocketsClient();
	~SocketsClient();

	void set_video_event(Video* pEvent);
	bool connect(const string& url, bool blocking, bool ssl);
	void stop();
	void reset();
	bool is_connected();
	string get_controller();
	string get_publisher();

	int send_msg(unsigned char* payload, unsigned int msglen);
	void continue_show_stream();
	void set_start_stream_callback(StartStreamCallback on_stream);
	void set_stop_stream_callback(StopStreamCallback on_stop);
	void set_quit_stream_callback(QuitStreamCallback on_quit);
	void handle_in(struct lws *wsi, const void* data, size_t len);

	void send_connect(const string& host_name, bool is_controller);
	void send_start_stream(const string& user_name);
	void send_stop_stream();
	void send_video_data(void* data);
	void send_video_ack(unsigned int sequence);
	void send_keyframe_request(bool reset_seq);
	void send_change_controller(const string& user_name);

	static int callback_client(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

private:
	void NotifyForceExit();
	void SetConnectState(int connstate);
	int RunWebSocketClient();
	unsigned int CalcSVCFrameSize(void* data);
	void SendStartStreamAck(bool success);
	void SendStopStreamAck();

	bool m_Exit;
	int  m_State;
	bool m_UseSSL;
	bool m_WaitingKeyframe;
	string m_ServerUrl;
	string m_Controller;
	string m_Publisher;

	lws* m_wsi;
	thread* m_wsthread;
	condition_variable m_cv;
	struct lws_protocols* m_protocols;

	Buffer* m_SendBuf;
	Video* m_pVideo;
	StartStreamCallback m_CallbackStream;
	StopStreamCallback m_CallbackStop;
	QuitStreamCallback m_CallbackQuit;
};
