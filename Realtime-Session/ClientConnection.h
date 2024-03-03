#ifndef __CLIENT_CONNECTION_H__
#define __CLIENT_CONNECTION_H__

#include "Session.h"

class ClientConnection
{
public:
    ClientConnection(string user_name, struct lws *wsi, Session *session);
    ~ClientConnection();

	void process_heartbeat();
	void process_start_stream();
	void process_start_stream_ack(uint8_t* data, int len);
	void process_video_data(uint8_t* data, int len, struct lws *wsi);
	void process_video_ack(uint8_t* data, int len);
	void process_request_key_frame(uint8_t* data, int len);
	void process_stop_stream();
	void process_stop_stream_ack();
	void process_audio_data(uint8_t* data, int len);
	void process_stream_quit();
	void process_message(uint8_t* data, int len, struct lws *wsi);

    struct lws* get_websocket_handle()
	{
		return m_wsi;
	}
	bool is_publisher()
	{
		return m_bPublisher;
	}
	void set_publisher(bool is_publisher)
	{
        m_bPublisher = is_publisher;
	}
	bool is_controller()
	{
		return m_bController;
	}
	void set_controller(bool is_controller)
	{
        m_bController = is_controller;
	}
	string get_user_name()
	{
		return m_strUserName;
	}
	void set_buffer_clear(bool bClear)
	{
	    m_bClear = bClear;
	}

private:
	void send_data(uint8_t* data, int len);

    struct lws *m_wsi;
	bool m_bClear;
	uint64_t m_uLastKeyRequestTime;
    Session *m_pSession;

    bool m_bPublisher;
    bool m_bController;
    string m_strUserName;
	uint8_t m_sSendBuf[LWS_PRE + COMMAND_BUFFER_SIZE];
};

#endif
