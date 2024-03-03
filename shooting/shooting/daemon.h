// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 DAEMON_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// DAEMON_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef DAEMON_EXPORTS
#define DAEMON_API __declspec(dllexport)
#else
#define DAEMON_API __declspec(dllimport)
#endif

#include <string>
#include <thread>
#include <functional>

typedef std::function<void(void)> StartStreamCallback;
typedef std::function<void(const std::string&)> StopStreamCallback;
typedef std::function<void(void)> QuitStreamCallback;

class SocketsClient;
class Video;
// 此类是从 dll 导出的
class DAEMON_API Cdaemon {
public:
	Cdaemon(const std::string& url, const std::string& host_name, bool is_controller);
	// TODO: 在此处添加方法。
	~Cdaemon();

	void start_stream(const std::string& user_name);
	void stop_stream();
	void show_stream(void* hWnd);
	std::string get_controller();
	std::string get_publisher();

	void send_message();
	void change_controller(const std::string& user_name);

	void set_start_stream_callback(StartStreamCallback on_stream);
	void set_stop_stream_callback(StopStreamCallback on_stop);
	void set_quit_stream_callback(QuitStreamCallback on_quit);

private:
	void OnVideoEncoded(void* data);
	void HeartbeatThread();

	SocketsClient* m_pMcuClient;
	Video* m_pVideo;
	std::string m_sMcuUrl;
	std::string m_sHostName;
	std::thread* m_pHeartbeatID;
	bool m_bQuit;
};

