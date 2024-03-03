#include "ClientConnection.h"

ClientConnection::ClientConnection(string user_name, struct lws *wsi, Session *session)
{
    m_wsi = wsi;
	m_bClear = false;
	m_uLastKeyRequestTime = 0;
    m_pSession = session;
	m_bPublisher = false;
	m_bController = false;
	m_strUserName = user_name;
}

ClientConnection::~ClientConnection()
{

}

void ClientConnection::process_heartbeat()
{
	string hearbeat_info = m_pSession->get_heartbeat_info();
	utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeHeartbeat, (uint8_t*)hearbeat_info.c_str(), hearbeat_info.size());
	int ret = lws_write(m_wsi, (unsigned char*)m_sSendBuf + LWS_PRE, hearbeat_info.size() + sizeof(MsgHeader), LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
	}
}

void ClientConnection::process_start_stream()
{
    m_bPublisher = true;
    utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeStartStream, NULL, 0);
    int ret = lws_write(m_wsi, (unsigned char*)m_sSendBuf + LWS_PRE, sizeof(MsgHeader), LWS_WRITE_BINARY);
    if (ret < 0) {
        LOG_ERROR("%s send msg error", __func__);
    }
}

void ClientConnection::process_start_stream_ack(uint8_t* data, int len)
{
	int ret = lws_write(m_wsi, (unsigned char*)(data - sizeof(MsgHeader)), len + sizeof(MsgHeader), LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
	}
}

void ClientConnection::process_video_data(uint8_t* data, int len, struct lws *wsi)
{
	VideoDataHeader* video_header = (VideoDataHeader*)data;
	if (m_bClear) {
		int frame_type = ntohl(video_header->eFrameType);
		LOG_DEBUG("frame type %d", frame_type);
		if (frame_type == videoFrameTypeIDR) {
			LOG_DEBUG("clear buffer stop");
			m_bClear = false;
			send_data(data, len);
		}
		else {
			LOG_DEBUG("clear buffer and send video ack");
			utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeVideoAck, (uint8_t*)&video_header->sequence, sizeof(unsigned int));
			int ret = lws_write(wsi, (unsigned char*)m_sSendBuf + LWS_PRE, sizeof(MsgHeader) + sizeof(unsigned int), LWS_WRITE_BINARY);
			if (ret < 0) {
				LOG_ERROR("%s send msg error", __func__);
			}
		}
	}
	else {
		LOG_DEBUG("send video data frametype=%d sequence=%d", ntohl(video_header->eFrameType), ntohl(video_header->sequence));
		send_data(data, len);
	}
}

void ClientConnection::process_video_ack(uint8_t* data, int len)
{
	int ret = lws_write(m_wsi, (unsigned char*)(data - sizeof(MsgHeader)), len + sizeof(MsgHeader), LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
	}
}

void ClientConnection::process_request_key_frame(uint8_t* data, int len)
{
	int ret = lws_write(m_wsi, (unsigned char*)(data - sizeof(MsgHeader)), len + sizeof(MsgHeader), LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
		return;
	}
#ifdef WIN32
	LARGE_INTEGER time;
	LARGE_INTEGER frequency;

	time.QuadPart = 0;
	frequency.QuadPart = 1;

	QueryPerformanceCounter(&time);
	QueryPerformanceFrequency(&frequency);
	uint64_t time_now = time.QuadPart * 1000 / frequency.QuadPart;
#else
	struct timeval time;
	gettimeofday(&time, NULL);
	uint64_t time_now = time.tv_sec * 1000 + time.tv_usec / 1000;
#endif
	if (time_now - m_uLastKeyRequestTime < 500 && m_uLastKeyRequestTime != 0) {
		LOG_DEBUG("need clear buffer");
		m_pSession->set_buffer_clear(true);
	}
	m_uLastKeyRequestTime = time_now;
}

void ClientConnection::process_stop_stream()
{
    m_bPublisher = false;
    utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeStopStream, NULL, 0);
    int ret = lws_write(m_wsi, (unsigned char*)m_sSendBuf + LWS_PRE, sizeof(MsgHeader), LWS_WRITE_BINARY);
    if (ret < 0) {
        LOG_ERROR("%s send msg error", __func__);
    }
}

void ClientConnection::process_stop_stream_ack()
{
    utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeStopStreamAck, NULL, 0);
    int ret = lws_write(m_wsi, (unsigned char*)m_sSendBuf + LWS_PRE, sizeof(MsgHeader), LWS_WRITE_BINARY);
    if (ret < 0) {
        LOG_ERROR("%s send msg error", __func__);
    }
}

void ClientConnection::process_audio_data(uint8_t* data, int len)
{
	LOG_DEBUG("send audio data");
	int ret = lws_write(m_wsi, (unsigned char*)(data - sizeof(MsgHeader)), len + sizeof(MsgHeader), LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
	}
}

void ClientConnection::process_stream_quit()
{
    LOG_DEBUG("send stream quit");
    if (m_bPublisher) {
        m_bPublisher = false;
    } else {
        utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeStreamQuit, NULL, 0);
        int ret = lws_write(m_wsi, (unsigned char*)m_sSendBuf + LWS_PRE, sizeof(MsgHeader), LWS_WRITE_BINARY);
        if (ret < 0) {
            LOG_ERROR("%s send msg error", __func__);
        }
    }
}

void ClientConnection::process_message(uint8_t* data, int len, struct lws *wsi)
{
    int ret = 0;
    if (m_wsi == wsi) {
        utils_create_send_buffer(m_sSendBuf, sizeof(m_sSendBuf), kMsgTypeMessageAck, NULL, 0);
        ret = lws_write(m_wsi, (unsigned char*)m_sSendBuf + LWS_PRE, sizeof(MsgHeader), LWS_WRITE_BINARY);
    } else {
        ret = lws_write(m_wsi, (unsigned char*)(data - sizeof(MsgHeader)), len + sizeof(MsgHeader), LWS_WRITE_BINARY);
    }
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
	}
}

void ClientConnection::send_data(uint8_t* data, int len)
{
	int ret = lws_write(m_wsi, (unsigned char*)(data - sizeof(MsgHeader)), len + sizeof(MsgHeader), LWS_WRITE_BINARY);
	if (ret < 0) {
		LOG_ERROR("%s send msg error", __func__);
	}
}
