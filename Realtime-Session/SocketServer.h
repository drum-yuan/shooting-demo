#ifndef __SOCKET_SERVER_H__
#define __SOCKET_SERVER_H__

#include "Utils.h"
#include "Session.h"
#include <thread>

class SocketServer
{
public:
    SocketServer(int port);
    ~SocketServer();

    void start();
    void stop();
    void run_websocket_server();
    void register_server_observer(SocketServerObserver* observer);

    static int ws_service_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

private:
    void process_message_received(struct lws *wsi, void* in, size_t len);
	void process_connection_closed(struct lws *wsi);

	SocketServerObserver* m_pServerObserver;
    int m_port;
    bool m_quit;
    thread* m_wsthread;
	struct lws_protocols* m_protocols;
};

#endif // __SOCKET_SERVER_H__
