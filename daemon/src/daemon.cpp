// daemon.cpp : 定义 DLL 的导出函数。
//

#include "sockets-client.h"
#include "daemon.h"
#include "util.h"


ofstream log_file;

// 这是已导出类的构造函数。
Cdaemon::Cdaemon(const string& url, const string& host_name, bool is_controller)
{
	if (!log_file.is_open()) {
		log_file.open("daemon.log", ios_base::out | ios_base::app);
	}
	_mkdir("video");
	m_pMcuClient = new SocketsClient();
	m_pVideo = new Video();
	m_sMcuUrl = url;
	m_sHostName = host_name;
	if (!m_pMcuClient->connect(url, true, false)) {
		LOG_ERROR("connect mcu failed");
		return;
	}

	m_pVideo->set_on_encoded(std::bind(&Cdaemon::OnVideoEncoded, this, std::placeholders::_1));
	m_pMcuClient->set_video_event(m_pVideo);
	m_pMcuClient->send_connect(host_name, is_controller);
	m_bQuit = false;
	m_pHeartbeatID = new thread(&Cdaemon::HeartbeatThread, this);
}

Cdaemon::~Cdaemon()
{
	m_bQuit = true;
	if (m_pHeartbeatID) {
		if (m_pHeartbeatID->joinable()) {
			m_pHeartbeatID->join();
			delete m_pHeartbeatID;
		}
		m_pHeartbeatID = NULL;
	}
	if (m_pVideo->is_publisher()) {
		m_pVideo->stop();
	}
	m_pMcuClient->stop();

	delete m_pVideo;
	delete m_pMcuClient;
	log_file.close();
}

void Cdaemon::start_stream(const std::string& user_name)
{
	m_pMcuClient->send_start_stream(user_name);
}

void Cdaemon::stop_stream()
{
	m_pMcuClient->send_stop_stream();
}

void Cdaemon::show_stream(void* hWnd)
{
	m_pVideo->set_render_win(hWnd);
	//m_pMcuClient->send_keyframe_request(true);
}

std::string Cdaemon::get_controller()
{
	return m_pMcuClient->get_controller();
}

std::string Cdaemon::get_publisher()
{
	return m_pMcuClient->get_publisher();
}

void Cdaemon::send_message()
{

}

void Cdaemon::change_controller(const std::string& user_name)
{
	m_pMcuClient->send_change_controller(user_name);
}

void Cdaemon::set_start_stream_callback(StartStreamCallback on_stream)
{
	m_pMcuClient->set_start_stream_callback(on_stream);
}

void Cdaemon::set_stop_stream_callback(StopStreamCallback on_stop)
{
	m_pMcuClient->set_stop_stream_callback(on_stop);
}

void Cdaemon::set_quit_stream_callback(QuitStreamCallback on_quit)
{
	m_pMcuClient->set_quit_stream_callback(on_quit);
}

void Cdaemon::OnVideoEncoded(void* data)
{
	m_pMcuClient->send_video_data(data);
}

void Cdaemon::HeartbeatThread()
{
	int retry = 3;

	while (!m_bQuit)
	{
		if (!m_pMcuClient->is_connected() && retry)
		{
			if (!m_pMcuClient->connect(m_sMcuUrl, true, false))
			{
				LOG_ERROR("reconnect mcu failed");
				if (!--retry)
				{
					break;
				}
			}
			else
			{
				m_pMcuClient->send_connect(m_sHostName, false);
				if (!m_pVideo->is_publisher()) {
					m_pMcuClient->send_keyframe_request(true);
					m_pMcuClient->continue_show_stream();
				}
				retry = 3;
			}
		}
		Sleep(100);
	}
}