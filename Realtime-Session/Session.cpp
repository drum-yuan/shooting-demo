#include "Session.h"
#include "SocketServer.h"
#include "ClientConnection.h"
#include "Json.h"
#include <signal.h>

static Session* _instance = NULL;
Session::Session(int port)
{
    m_pSocketServer = new SocketServer(port);
    m_bExit = true;
    _instance = this;
}

Session::~Session()
{
    m_bExit = true;
	if (m_pSocketServer) {
		delete m_pSocketServer;
		m_pSocketServer = NULL;
	}
}

bool Session::init()
{
    m_pSocketServer->register_server_observer(this);
    m_pSocketServer->start();
    return true;
}

void Session::run()
{
    m_bExit = false;
    while (!m_bExit) {
        process_heartbeat();
		utils_sleep(2000);
    }
    m_pSocketServer->stop();
}

void Session::process_heartbeat()
{
	if (m_pClientConnectionVector.empty()) {
		return;
	}
	LOG_DEBUG("process_heartbeat");
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		(*it)->process_heartbeat();
	}
}

void Session::process_start_stream(uint8_t* data, int len)
{
    string user_name;
    if (len > 0) {
        Json::Reader reader;
        Json::Value root;
        string str_data = string((char*)data, len);
        if (!str_data.empty() && reader.parse(str_data, root))
        {
            user_name = root["UserName"].asString();
        }
    }
    LOG_DEBUG("process_start_stream %s", user_name.c_str());
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
        if (user_name == (*it)->get_user_name()) {
            (*it)->process_start_stream();
            break;
        }
	}
}

void Session::process_start_stream_ack(uint8_t* data, int len)
{
	LOG_DEBUG("process_start_stream_ack");
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if ((*it)->is_controller()) {
			(*it)->process_start_stream_ack(data, len);
			break;
		}
	}
}

void Session::process_video_data(uint8_t* data, int len, struct lws *wsi)
{
	LOG_DEBUG("process_video_data len=%d", len);
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if (!((*it)->is_publisher())) {
			(*it)->process_video_data(data, len, wsi);
		}
	}
}

void Session::process_video_ack(uint8_t* data, int len)
{
	LOG_DEBUG("process_video_ack len=%d", len);
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if ((*it)->is_publisher()) {
			(*it)->process_video_ack(data, len);
		}
	}
}

void Session::process_request_key_frame(uint8_t* data, int len)
{
	LOG_DEBUG("process_request_key_frame len=%d", len);
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if ((*it)->is_publisher()) {
			(*it)->process_request_key_frame(data, len);
		}
	}
}

void Session::process_stop_stream()
{
	LOG_DEBUG("process_stop_stream");
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if ((*it)->is_publisher()) {
			(*it)->process_stop_stream();
		}
	}
}

void Session::process_stop_stream_ack()
{
	LOG_DEBUG("process_stop_stream_ack");
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if ((*it)->is_controller()) {
			(*it)->process_stop_stream_ack();
			break;
		}
	}
}

void Session::process_audio_data(uint8_t* data, int len)
{
	LOG_DEBUG("process_audio_data");
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if (!((*it)->is_publisher())) {
			(*it)->process_audio_data(data, len);
		}
	}
}

void Session::process_stream_quit()
{
    LOG_WARN("notify reciever stream quit");
    for (vector<ClientConnection*>::iterator it = _instance->m_pClientConnectionVector.begin(); it != _instance->m_pClientConnectionVector.end(); it++)
    {
        (*it)->process_stream_quit();
    }
}

void Session::process_message(uint8_t* data, int len, struct lws *wsi)
{
    LOG_DEBUG("process_message");
    for (vector<ClientConnection*>::iterator it = _instance->m_pClientConnectionVector.begin(); it != _instance->m_pClientConnectionVector.end(); it++)
    {
        (*it)->process_message(data, len, wsi);
    }
}

void Session::process_assign_controller(uint8_t* data, int len)
{
    string user_name;
    if (len > 0) {
        Json::Reader reader;
        Json::Value root;
        string str_data = string((char*)data, len);
        if (!str_data.empty() && reader.parse(str_data, root))
        {
            user_name = root["UserName"].asString();
        }
    }
    LOG_DEBUG("process_assign_controller %s", user_name.c_str());
    for (vector<ClientConnection*>::iterator it = _instance->m_pClientConnectionVector.begin(); it != _instance->m_pClientConnectionVector.end(); it++)
    {
        if (user_name == (*it)->get_user_name()) {
            (*it)->set_controller(true);
        } else {
            (*it)->set_controller(false);
        }
    }
}

string Session::get_heartbeat_info()
{
    Json::FastWriter writer;
    Json::Value root;
    vector<string> vecUsers;
    string publisher;
    string controller;
    for (vector<ClientConnection*>::iterator it = _instance->m_pClientConnectionVector.begin(); it != _instance->m_pClientConnectionVector.end(); it++)
    {
        vecUsers.push_back((*it)->get_user_name());
        if ((*it)->is_publisher()) {
            publisher = (*it)->get_user_name();
        }
        if ((*it)->is_controller()) {
            controller = (*it)->get_user_name();
        }
    }
    root["UserNum"] = (int)vecUsers.size();
    for (int i = 0; i < (int)vecUsers.size(); i++) {
        root["UserList"][i] = vecUsers[i];
    }
    root["Publisher"] = publisher;
    root["Controller"] = controller;
    LOG_DEBUG("controller: %s, publisher: %s", controller.c_str(), publisher.c_str());
    return writer.write(root);
}

void Session::set_buffer_clear(bool bClear)
{
	for (vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin(); it != m_pClientConnectionVector.end(); it++)
	{
		if (!((*it)->is_publisher())) {
			(*it)->set_buffer_clear(bClear);
		}
	}
}

void Session::on_connection_closed(struct lws *wsi)
{
    LOG_DEBUG("on_connection_closed. wsi %p", wsi);

    bool change_controller = false;
    vector<ClientConnection*>::iterator it = m_pClientConnectionVector.begin();
    while (it != m_pClientConnectionVector.end()) {
        if ((*it)->get_websocket_handle() == wsi) {
            if ((*it)->is_controller()) {
                change_controller = true;
            }
            delete *it;
            it = m_pClientConnectionVector.erase(it);
            break;
        }
        it++;
    }

    if (m_pClientConnectionVector.empty()) {
        signal(SIGALRM, process_exit);
        alarm(30);
    } else {
        LOG_INFO("change controller");
        if (change_controller) {
            it = m_pClientConnectionVector.begin();
            (*it)->set_controller(true);
        }
    }
}

void Session::on_connection_opened(string& user_name, bool is_controller, bool is_publisher, struct lws *wsi)
{
    LOG_DEBUG("on_connection_opened");
    ClientConnection* pConnection = new ClientConnection(user_name, wsi, this);
    m_pClientConnectionVector.push_back(pConnection);
    pConnection->set_controller(is_controller);
    pConnection->set_publisher(is_publisher);
}

void Session::on_message(int type, uint8_t* data, int len, struct lws *wsi)
{
    switch (type) {
    case kMsgTypeStartStream:
		process_start_stream(data, len);
		break;
    case kMsgTypeStartStreamAck:
		process_start_stream_ack(data, len);
		break;
    case kMsgTypeVideoData:
		process_video_data(data, len, wsi);
		break;
    case kMsgTypeVideoAck:
		process_video_ack(data, len);
		break;
    case kMsgTypeRequestKeyFrame:
		process_request_key_frame(data, len);
		break;
    case kMsgTypeStopStream:
		process_stop_stream();
		break;
    case kMsgTypeStopStreamAck:
		process_stop_stream_ack();
		break;
    case kMsgTypeAudioData:
		process_audio_data(data, len);
		break;
    case kMsgTypeStreamQuit:
        process_stream_quit();
        break;
    case kMsgTypeMessage:
        process_message(data, len, wsi);
        break;
    case kMsgTypeAssignController:
        process_assign_controller(data, len);
        break;
    default:
		LOG_WARN("unknown message type %d", type);
        break;
    }
}

void Session::process_exit(int param)
{
    LOG_WARN("publisher disconnect timeout");
    _instance->m_bExit = true;
}


