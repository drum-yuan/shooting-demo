#ifndef __SESSION_H__
#define __SESSION_H__

#include "Utils.h"
#include "Protocol.h"
#include <vector>
#include <map>


class SocketServer;
class SocketClient;
class ClientConnection;
class SocketServerObserver
{
public:
    virtual void on_connection_closed(struct lws *wsi) = 0;
    virtual void on_connection_opened(string& user_name, bool is_controller, bool is_publisher, struct lws *wsi) = 0;
	virtual void on_message(int type, uint8_t* data, int len, struct lws *wsi) = 0;
};

class Session : public SocketServerObserver
{
public:
    Session(int port);
    virtual ~Session();
    bool init();
    void run();

	void process_heartbeat();
	void process_start_stream(uint8_t* data, int len);
	void process_start_stream_ack(uint8_t* data, int len);
	void process_video_data(uint8_t* data, int len, struct lws *wsi);
	void process_video_ack(uint8_t* data, int len);
	void process_request_key_frame(uint8_t* data, int len);
	void process_stop_stream();
	void process_stop_stream_ack();
	void process_audio_data(uint8_t* data, int len);
	void process_stream_quit();
	void process_message(uint8_t* data, int len, struct lws *wsi);
	void process_assign_controller(uint8_t* data, int len);

	string get_heartbeat_info();
	void set_buffer_clear(bool bClear);

    virtual void on_connection_closed(struct lws *wsi);
    virtual void on_connection_opened(string& user_name, bool is_controller, bool is_publisher, struct lws *wsi);
	virtual void on_message(int type, uint8_t* data, int len, struct lws *wsi);

	static void process_exit(int param);

private:
    SocketServer* m_pSocketServer;
    SocketClient* m_pSocketClient;
    vector<ClientConnection*> m_pClientConnectionVector;
    bool m_bExit;
};

#endif // __SESSION_H__
