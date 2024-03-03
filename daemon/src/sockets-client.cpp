#include "protocol.h"
#include "proto.hpp"
#include "autojsoncxx/autojsoncxx.hpp"
#include "libyuv.h"
#include "sockets-client.h"
#include "util.h"

#define WEBSOCKET_MAX_BUFFER_SIZE (1024 * 1024 * 2)
#define CONNECT_TIMEOUT 5000  //ms

using namespace MCUProtocol;

enum ClientConnState {
	ConnectStateUnConnected = 0,
	ConnectStateConnecting = 1,
	ConnectStateConnectError = 2,
	ConnectStateEstablished = 3,
	ConnectStateClosed = 4
};

SocketsClient::SocketsClient() : m_Exit(false),
								m_State(ConnectStateUnConnected),
								m_UseSSL(false),
								m_WaitingKeyframe(false),
								m_wsi(NULL),
								m_wsthread(NULL),
								m_protocols(NULL)
{
	m_protocols = (struct lws_protocols*)malloc(sizeof(struct lws_protocols) * 2);
	if (m_protocols)
	{
		memset(m_protocols, 0, sizeof(struct lws_protocols) * 2);
		m_protocols[0].name = "binary";
		m_protocols[0].callback = SocketsClient::callback_client;
		m_protocols[0].per_session_data_size = sizeof(SocketsClient*);
		m_protocols[0].rx_buffer_size = WEBSOCKET_MAX_BUFFER_SIZE + LWS_PRE;
		m_protocols[0].user = this;

		m_protocols[1].name = NULL;
		m_protocols[1].callback = NULL;
		m_protocols[1].per_session_data_size = 0;
		m_protocols[1].rx_buffer_size = 0;
		m_protocols[1].user = NULL;
	}

	m_SendBuf = new Buffer(WEBSOCKET_MAX_BUFFER_SIZE);
	m_pVideo = NULL;
	m_CallbackStream = NULL;
	m_CallbackStop = NULL;
	m_CallbackQuit = NULL;
}

SocketsClient::~SocketsClient()
{
	stop();
	if (m_protocols)
	{
		free(m_protocols);
		m_protocols = NULL;
	}

	delete m_SendBuf;
}

void SocketsClient::NotifyForceExit()
{
	m_Exit = true;
}

void SocketsClient::SetConnectState(int connstate)
{
	m_State = connstate;
	m_cv.notify_all();
}

int SocketsClient::RunWebSocketClient()
{
	int n = 0;
	int port = 0;
	int use_ssl = 0;
	const char *prot, *p, *address;
	struct lws_context *context;
	struct lws_context_creation_info info;
	char uri[256] = "/";
	char ads_port[256 + 30];
	struct lws_client_connect_info i;
	int debug_level = 7;

	memset(&info, 0, sizeof info);
	if (m_UseSSL)
		use_ssl = 1;

	if (!m_protocols)
		return -1;

	/* tell the library what debug level to emit and to send it to syslog */
	lws_set_log_level(debug_level, lwsl_emit_syslog);

	if (lws_parse_uri((char*)m_ServerUrl.c_str(), &prot, &address, &port, &p))
		return -1;

	uri[0] = '/';
	strncpy(uri + 1, p, sizeof(uri) - 2);
	uri[sizeof(uri) - 1] = '\0';
	i.path = uri;

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.iface = NULL;
	info.protocols = m_protocols;
	info.keepalive_timeout = PENDING_TIMEOUT_HTTP_KEEPALIVE_IDLE;

	if (use_ssl) {
		info.ssl_cert_filepath = NULL;
		info.ssl_private_key_filepath = NULL;
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	}

	info.gid = -1;
	info.uid = -1;
	info.options |= LWS_SERVER_OPTION_VALIDATE_UTF8;

	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}

	n = 0;
	while (n >= 0 && !m_Exit) {
		if (!m_State) {
			m_State = ConnectStateConnecting;
			lwsl_notice("Client connecting to %s:%u....\n", address, port);

			sprintf(ads_port, "%s:%u", address, port & 65535);
			memset(&i, 0, sizeof(i));
			i.context = context;
			i.address = address;
			i.port = port;
			i.ssl_connection = use_ssl;
			i.path = uri;
			i.host = ads_port;
			i.origin = ads_port;
			i.protocol = "binary";
			i.userdata = this;

			m_wsi = lws_client_connect_via_info(&i);
			if (!m_wsi) {
				lwsl_err("Client failed to connect to %s:%u\n", address, port);
				goto fail;
			}
		}
		lws_callback_on_writable_all_protocol(context, &m_protocols[0]);
		n = lws_service(context, 20);
	}
fail:
	lws_context_destroy(context);
	lwsl_notice("websocket client exited cleanly\n");

	return 0;
}

void SocketsClient::set_video_event(Video* pEvent)
{
	m_pVideo = pEvent;
}

bool SocketsClient::connect(const string& url, bool blocking, bool ssl)
{
	m_ServerUrl = url;
	m_UseSSL = ssl;
	reset();
	m_Exit = false;
	m_wsthread = new std::thread(&SocketsClient::RunWebSocketClient, this);

	if (blocking && m_State != ConnectStateEstablished)
	{
		std::mutex mtx;
		std::unique_lock<std::mutex> lck(mtx);
		while (m_cv.wait_for(lck, std::chrono::milliseconds(CONNECT_TIMEOUT)) == std::cv_status::timeout) {
			LOG_ERROR("websocket connect timeout");
			NotifyForceExit();
			return false;
		}
		if (m_State == ConnectStateClosed || m_State == ConnectStateConnectError)
		{
			LOG_ERROR("websocket connect failed");
			return false;
		}
	}

	return true;
}

void SocketsClient::reset()
{
	m_Exit = true;
	if (m_wsthread)
	{
		if (m_wsthread->joinable())
		{
			m_wsthread->join();
			delete m_wsthread;
		}
		m_wsthread = NULL;
	}
	m_wsi = NULL;
	m_State = ConnectStateUnConnected;
}

void SocketsClient::stop()
{
	reset();
}

bool SocketsClient::is_connected()
{
	return m_State == ConnectStateEstablished;
}

string SocketsClient::get_controller()
{
	return m_Controller;
}

string SocketsClient::get_publisher()
{
	return m_Publisher;
}

int SocketsClient::send_msg(unsigned char* payload, unsigned int msglen)
{
	if (m_State != ConnectStateEstablished)
	{
		return -1;
	}

	int ret = lws_write(m_wsi, payload, msglen, LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("send msg error");
	}
	return ret;
}

void SocketsClient::continue_show_stream()
{
	if (m_CallbackStream) {
		m_CallbackStream();
	}
}

void SocketsClient::set_start_stream_callback(StartStreamCallback on_stream)
{
	m_CallbackStream = on_stream;
}

void SocketsClient::set_stop_stream_callback(StopStreamCallback on_stop)
{
	m_CallbackStop = on_stop;
}

void SocketsClient::set_quit_stream_callback(QuitStreamCallback on_quit)
{
	m_CallbackQuit = on_quit;
}

void SocketsClient::handle_in(struct lws *wsi, const void* in, size_t len)
{
	WebSocketHeader* pWebHeader = (WebSocketHeader*)in;
	unsigned short uType = Swap16IfLE(pWebHeader->type);
	unsigned int uPayloadLen = Swap32IfLE(pWebHeader->length);

	//LOG_DEBUG("handle in type %u\n", uType);
	switch (uType)
	{
	case kMsgTypeStartStream:
	{
		if (m_pVideo) {
			m_pVideo->start();
			SendStartStreamAck(true);
		}
		else {
			SendStartStreamAck(false);
		}
	}
		break;
	case kMsgTypeStartStreamAck:
	{
		if (m_CallbackStream) {
			m_CallbackStream();
		}
	}
		break;
	case kMsgTypeVideoData:
	{
		if (m_pVideo == NULL) {
			break;
		}
		VideoDataHeader* pVideoHeader = (VideoDataHeader*)((uint8_t*)in + sizeof(WebSocketHeader));
		unsigned int sequence = Swap32IfLE(pVideoHeader->sequence);
		unsigned int option = Swap32IfLE(pVideoHeader->option);
		unsigned int len1 = uPayloadLen - sizeof(VideoDataHeader);
		unsigned int len2 = 0;
		//printf("recv length %u\n", uPayloadLen + sizeof(WebSocketHeader));

		if (option > 2) {
			len2 = len1 - option;
			len1 = option;
		}
		else if (option == 2) {
			len2 = len1;
			len1 = 0;
		}
		uint8_t* p = (uint8_t*)in + sizeof(WebSocketHeader) + sizeof(VideoDataHeader);
		unsigned int offset = 0;
		unsigned int rect_num = 0;
		if (sequence != 0xffffffff) {
			send_video_ack(sequence);
		}
		if (len1 > 0) {
			if (option != 0) {
				rect_num = *(unsigned int*)p;
				offset = rect_num * 8 + 4;
			}
			LOG_DEBUG("decode 1 offset=%d len=%d", offset, len1 - offset);
			if (!m_pVideo->show(p + offset, len1 - offset, true)) {
				if (!m_WaitingKeyframe) {
					send_keyframe_request(false);
					m_WaitingKeyframe = true;
				}
			}
			else {
				m_WaitingKeyframe = false;
			}
		}
		if (len2 > 0) {
			rect_num = *(unsigned int*)(p + len1);
			offset = rect_num * 8 + 4;
			LOG_DEBUG("decode 2 offset=%d len=%d", offset, len2 - offset);
			m_pVideo->show(p + len1 + offset, len2 - offset, false);
		}
	}
		break;
	case kMsgTypeVideoAck:
	{
		if (m_pVideo == NULL) {
			break;
		}
		VideoAck_C2S tVideoAck;
		const char* pPayload = (const char*)in + sizeof(WebSocketHeader);
		autojsoncxx::ParsingResult result;
		if (!autojsoncxx::from_json_string(pPayload, tVideoAck, result)) {
			LOG_ERROR("parser json string fail");
			break;
		}
		cap_set_ack_sequence(tVideoAck.sequence);
	}
		break;
	case kMsgTypeRequestKeyFrame:
	{
		if (m_pVideo && m_pVideo->is_publisher()) {
			RequestKeyFrame tRequestKeyFrame;
			const char* pPayload = (const char*)in + sizeof(WebSocketHeader);
			autojsoncxx::ParsingResult result;
			if (!autojsoncxx::from_json_string(pPayload, tRequestKeyFrame, result)) {
				LOG_ERROR("parser json string fail");
				break;
			}
			bool is_reset = tRequestKeyFrame.resetSequence;
			m_pVideo->reset_keyframe(is_reset);
		}
	}
		break;
	case kMsgTypeStopStream:
	{
		if (m_pVideo && m_pVideo->is_publisher()) {
			if (m_CallbackStop) {
				m_CallbackStop(m_pVideo->get_raw_file_name());
			}
			m_pVideo->stop();
			SendStopStreamAck();
		}
	}
		break;
	case kMsgTypeStopStreamAck:
	{

	}
		break;
	case kMsgTypeStreamQuit:
	{
		if (m_CallbackQuit) {
			m_CallbackQuit();
		}
	}
		break;
	case kMsgTypeHeartbeat:
	{
		HeartbeatInfo tHeartbeat;
		const char* pPayload = (const char*)in + sizeof(WebSocketHeader);
		autojsoncxx::ParsingResult result;
		if (!autojsoncxx::from_json_string(pPayload, tHeartbeat, result)) {
			LOG_ERROR("parser json string fail %s", pPayload);
			break;
		}
		m_Controller = tHeartbeat.Controller;
		m_Publisher = tHeartbeat.Publisher;
	}
		break;
	default:
		LOG_ERROR("handle_in unknown msg type %d", uType);
		break;
	}
}

void SocketsClient::send_connect(const string& host_name, bool is_controller)
{
	string str = "{\"UserName\":\"";
	str += host_name;
	str += "\", \"IsController\":";
	if (is_controller) {
		str += "true, \"IsPublisher\":";
	}
	else {
		str += "false, \"IsPublisher\":";
	}
	if (m_pVideo && m_pVideo->is_publisher()) {
		str += "true}";
	}
	else {
		str += "false}";
	}
	
	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeConnect);
	header.length = Swap32IfLE(str.size());
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	m_SendBuf->append((unsigned char*)str.c_str(), str.size());

	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::send_start_stream(const string& user_name)
{
	string str = "{\"UserName\":\"";
	str += user_name;
	str += "\"}";

	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeStartStream);
	header.length = Swap32IfLE(str.size());
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	m_SendBuf->append((unsigned char*)str.c_str(), str.size());

	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::send_stop_stream()
{
	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeStopStream);
	header.length = 0;
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::send_video_data(void* data)
{
	if (data == NULL) {
		LOG_ERROR("send data null");
		return;
	}

	LOG_DEBUG("send video data");
	int iFrameSize = CalcSVCFrameSize(data);
	if (!iFrameSize) {
		LOG_INFO("frame size 0");
		cap_set_capture_sequence(cap_get_capture_sequence() - 1);
		return;
	}

	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.length = Swap32IfLE(sizeof(VideoDataHeader) + iFrameSize);
	header.type = Swap16IfLE(kMsgTypeVideoData);
	m_SendBuf->append((void*)&header, sizeof(WebSocketHeader));

	SFrameBSInfo* sFbi = (SFrameBSInfo*)data;
	VideoDataHeader videoheader;
	videoheader.eFrameType = Swap32IfLE(sFbi->eFrameType);
	unsigned int sequence = cap_get_capture_sequence();
	videoheader.sequence = Swap32IfLE(sequence);
	videoheader.option = 0;
	m_SendBuf->append((unsigned char*)&videoheader, sizeof(VideoDataHeader));

	int iLayer = 0;
	while (iLayer < sFbi->iLayerNum) {
		SLayerBSInfo* pLayerBsInfo = &(sFbi->sLayerInfo[iLayer]);
		if (pLayerBsInfo != NULL) {
			int iLayerSize = 0;
			int iNalIdx = pLayerBsInfo->iNalCount - 1;
			do {
				iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
				--iNalIdx;
			} while (iNalIdx >= 0);

			if (iLayerSize > 0) {
				
					/*unsigned char five_bits = pLayerBsInfo->pBsBuf[4] & 0x1f;
					if ((five_bits == 0x07) || (five_bits == 0x08)) { //sps or pps
						m_SendBuf->append((unsigned char*)pLayerBsInfo->pBsBuf, iLayerSize);
						m_pVideo->write_data((unsigned char*)pLayerBsInfo->pBsBuf, iLayerSize);
					}
					else {
						if (iLayer == 0) {
							m_SendBuf->append((unsigned char*)pLayerBsInfo->pBsBuf, iLayerSize);
						}
						else {
							m_pVideo->write_data((unsigned char*)pLayerBsInfo->pBsBuf, iLayerSize);
						}
					}*/

				m_SendBuf->append((unsigned char*)pLayerBsInfo->pBsBuf, iLayerSize);
				m_pVideo->write_data((unsigned char*)pLayerBsInfo->pBsBuf, iLayerSize);
			}
		}
		++iLayer;
	}
	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::send_video_ack(unsigned int sequence)
{
	VideoAck_C2S tVideoAck;
	tVideoAck.sequence = sequence;
	string str = autojsoncxx::to_json_string(tVideoAck);
	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeVideoAck);
	header.length = Swap32IfLE(str.size());
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	m_SendBuf->append((unsigned char*)str.c_str(), str.size());
	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::send_keyframe_request(bool reset_seq)
{
	RequestKeyFrame tRequest;
	tRequest.resetMcuBuf = false;
	tRequest.resetSequence = reset_seq;
	string str = autojsoncxx::to_json_string(tRequest);
	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeRequestKeyFrame);
	header.length = Swap32IfLE(str.size());
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	m_SendBuf->append((unsigned char*)str.c_str(), str.size());
	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::send_change_controller(const string& user_name)
{
	string str = "{\"UserName\":\"";
	str += user_name;
	str += "\"}";

	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeAssignController);
	header.length = Swap32IfLE(str.size());
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	m_SendBuf->append((unsigned char*)str.c_str(), str.size());

	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

int SocketsClient::callback_client(struct lws *wsi, enum lws_callback_reasons reason, void *user,
									void *in, size_t len)
{
	SocketsClient* wsclient = (SocketsClient*)user;

	switch (reason) {
		/* when the callback is used for client operations --> */

	case LWS_CALLBACK_CLOSED:
		lwsl_debug("closed\n");
		wsclient->SetConnectState(ConnectStateClosed);
		wsclient->NotifyForceExit();
		break;
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_debug("close with error, reconnect\n");
		wsclient->SetConnectState(ConnectStateConnectError);
		wsclient->NotifyForceExit();
		break;
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lwsl_debug("Client has connected\n");
		wsclient->SetConnectState(ConnectStateEstablished);
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
		//lwsl_notice("Client RX %d\n", len);
		wsclient->handle_in(wsi, in, len);
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		break;

	case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		/* reject everything else except permessage-deflate */
		if (strcmp((const char*)in, "permessage-deflate"))
			return 1;
		break;

	default:
		break;
	}

	return 0;
}

unsigned int SocketsClient::CalcSVCFrameSize(void* data)
{
	int iFrameSize = 0;
	int iLayer = 0;

	if (!data) {
		LOG_ERROR("encoded data null");
		return 0;
	}
	SFrameBSInfo* sFbi = (SFrameBSInfo*)data;
	while (iLayer < sFbi->iLayerNum) {
		SLayerBSInfo* pLayerBsInfo = &(sFbi->sLayerInfo[iLayer]);
		if (pLayerBsInfo != NULL) {
			int iLayerSize = 0;
			int iNalIdx = pLayerBsInfo->iNalCount - 1;
			do {
				iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
				--iNalIdx;
			} while (iNalIdx >= 0);

			if (iLayerSize)
			{
				iFrameSize += iLayerSize;
			}
		}
		++iLayer;
	}
	return iFrameSize;
}

void SocketsClient::SendStartStreamAck(bool success)
{
	string str = "{\"Success\":";
	if (success) {
		str += "true}";
	}
	else {
		str += "false}";
	}

	WebSocketHeader header;
	header.major = 1;
	header.minor = 0;
	header.type = Swap16IfLE(kMsgTypeVideoAck);
	header.length = Swap32IfLE(str.size());
	m_SendBuf->append(&header, sizeof(WebSocketHeader));
	m_SendBuf->append((unsigned char*)str.c_str(), str.size());

	send_msg((unsigned char*)m_SendBuf->getbuf(), m_SendBuf->getdatalength());
	m_SendBuf->reset();
}

void SocketsClient::SendStopStreamAck()
{

}