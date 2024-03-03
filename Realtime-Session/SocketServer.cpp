#include "SocketServer.h"
#include "Protocol.h"
#include "Json.h"
#include <string.h>

static SocketServer* s_server = NULL;
SocketServer::SocketServer(int port) : m_pServerObserver(NULL), m_port(port), m_quit(true), m_wsthread(NULL)
{
	m_protocols = (struct lws_protocols*)malloc(sizeof(struct lws_protocols) * 2);
	if (m_protocols)
	{
		memset(m_protocols, 0, sizeof(struct lws_protocols) * 2);
		m_protocols[0].name = "binary";
		m_protocols[0].callback = SocketServer::ws_service_callback;
		m_protocols[0].per_session_data_size = 0;
		m_protocols[0].rx_buffer_size = WEBSOCKET_MAX_BUFFER_SIZE + LWS_PRE;

		m_protocols[1].name = NULL;
		m_protocols[1].callback = NULL;
		m_protocols[1].per_session_data_size = 0;
		m_protocols[1].rx_buffer_size = 0;
	}
}

SocketServer::~SocketServer()
{
    stop();
}

int SocketServer::ws_service_callback(struct lws *wsi,
    enum lws_callback_reasons reason, void *user,
    void *in, size_t len)
{
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        LOG_INFO("established %p", wsi);
		break;
    case LWS_CALLBACK_RECEIVE:
        s_server->process_message_received(wsi, in, len);
		break;
	case LWS_CALLBACK_CLOSED:
        LOG_INFO("websocket closed %p", wsi);
		s_server->process_connection_closed(wsi);
		break;
    default:
		break;
	}

	return 0;
}

void SocketServer::start()
{
	s_server = this;
	m_quit = false;
    m_wsthread = new thread(&SocketServer::run_websocket_server, this);
}

void SocketServer::stop()
{
    m_quit = true;
	if (m_wsthread)
	{
		if (m_wsthread->joinable())
		{
			m_wsthread->join();
			delete m_wsthread;
		}
		m_wsthread = NULL;
	}
}

void SocketServer::run_websocket_server()
{
	struct lws_context_creation_info info;
	struct lws_context *context;
    int n = 0;
    int debug_level = 7;

    lws_set_log_level(debug_level, utils_ws_emit_log);

	memset(&info, 0, sizeof(info));
	info.port = m_port;
	info.iface = NULL;
	info.protocols = m_protocols;
	info.extensions = NULL;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.ssl_ca_filepath = NULL;
	info.ssl_cipher_list = NULL;
	info.gid = -1;
	info.uid = -1;
	info.options = 0;
	info.ka_time = 0;
	info.ka_probes = 0;
	info.ka_interval = 0;
	info.timeout_secs = 5;

	LOG_DEBUG("quit %d", m_quit);
	context = lws_create_context(&info);
	if (context == NULL) {
		LOG_ERROR("Websocket context create error");
		return;
	}
	LOG_DEBUG("quit %d", m_quit);
	while (n >= 0 && !m_quit) {
		//lws_callback_on_writable_all_protocol(context, m_protocols);
		n = lws_service(context, 10);
	}
	lws_context_destroy(context);
}

void SocketServer::register_server_observer(SocketServerObserver* observer)
{
	LOG_DEBUG("register server observer %p", observer);
    m_pServerObserver = observer;
}

void SocketServer::process_message_received(struct lws *wsi, void* in, size_t len)
{
    MsgHeader *header = (MsgHeader*)in;
    uint16_t type = ntohs(header->type);
    uint32_t length = ntohl(header->length);

	LOG_DEBUG("process_message_received type=%d length=%d", type, length);
    if (length + sizeof(MsgHeader) > len) {
        LOG_WARN("received data is not complete");
        return;
    }
    uint8_t* payload = (uint8_t*)in + sizeof(MsgHeader);
    switch (type) {
    case kMsgTypeConnect:
    {
		if (m_pServerObserver) {
			string user_name;
			bool is_controller = false;
			bool is_publisher = false;
			if (length > 0) {
				Json::Reader reader;
				Json::Value root;
				string str_payload = string((char*)payload, length);
				LOG_DEBUG("connect payload: %s", str_payload.c_str());
				if (!str_payload.empty() && reader.parse(str_payload, root))
				{
					user_name = root["UserName"].asString();
					is_controller = root["IsController"].asBool();
					is_publisher = root["IsPublisher"].asBool();
				}
			}
			LOG_DEBUG("received connect user_name %s, is controller %d, is publisher %d", user_name.c_str(), is_controller, is_publisher);
        	m_pServerObserver->on_connection_opened(user_name, is_controller, is_publisher, wsi);
		}
    }
    	break;
    case kMsgTypeStartStream:
    case kMsgTypeStartStreamAck:
    case kMsgTypeVideoData:
    case kMsgTypeVideoAck:
    case kMsgTypeRequestKeyFrame:
    case kMsgTypeAudioData:
    case kMsgTypeStreamQuit:
    case kMsgTypeStopStream:
    case kMsgTypeStopStreamAck:
    case kMsgTypeMessage:
    case kMsgTypeMessageAck:
    case kMsgTypeAssignController:
	{
		if (m_pServerObserver) {
        	m_pServerObserver->on_message(type, payload, length, wsi);
		}
	}
        break;
    default:
        break;
    }
}

void SocketServer::process_connection_closed(struct lws *wsi)
{
	if (m_pServerObserver) {
    	m_pServerObserver->on_connection_closed(wsi);
	}
}

