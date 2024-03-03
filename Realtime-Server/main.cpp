#include <restbed>
#include "Json.h"
#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <fstream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <uuid/uuid.h>


using namespace restbed;

typedef struct tagNodeInfo
{
    std::string host_name;
    std::string room_id;
} NodeInfo;

typedef struct tagInviteInfo
{
    std::string invite_name;
    std::string room_id;
} InviteInfo;

typedef struct tagRoomInfo
{
    std::string room_name;
    std::string room_id;
    int session_port;
    int session_pid;
    int status;
} RoomInfo;


static std::list<NodeInfo> _node_list;
static std::list<RoomInfo> _room_list;
static std::list<InviteInfo> _invite_list;
static std::mutex _room_lock;
static std::mutex _node_lock;
static int _current_port = 20000;


static void sigchld_handler(int sig)
{
    if (sig == SIGCHLD) {
        int status = 0;
        int pid = waitpid(-1, &status, WNOHANG);
        printf("recv sigchld pid %d\n", pid);
        _room_lock.lock();
        for (std::list<RoomInfo>::iterator it = _room_list.begin(); it != _room_list.end(); it++) {
            if ((*it).session_pid == pid) {
                (*it).status = 1;
                (*it).session_pid = -1;
                break;
            }
        }
        _room_lock.unlock();
    }
}

static std::string get_room_guid()
{
    uuid_t uuid;
    char str[36];

    uuid_generate(uuid);
    uuid_unparse(uuid, str);
    return std::string(str);
}

static pid_t start_session(int port)
{
    pid_t pid = vfork();
    if (pid == 0) {
        execl("./Realtime-Session", "Realtime-Session", std::to_string(port).c_str(), NULL);
        exit(0);
    } else {
        printf("pid=%d, getpid=%d\n", pid, getpid());
        signal(SIGCHLD, sigchld_handler);
    }
    return pid;
}

void post_add_node_handler(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request();

    printf("post_add_node_handler\n");
    int content_length = stoi(request->get_header("Content-Length"));
    session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
    {
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(std::string((const char*)body.data()), root))
        {
			session->close(NOT_ACCEPTABLE, "Data is not acceptable!");
            return;
        }

        std::string host_name;
        if (root["host_name"].isString()) {
            host_name = root["host_name"].asString();
        }
        printf("host_name %s\n", host_name.c_str());

        _node_lock.lock();
        std::list<NodeInfo>::iterator it = _node_list.begin();
        while (it != _node_list.end()) {
            if ((*it).host_name == host_name) {
                break;
            }
            it++;
        }
        if (it == _node_list.end()) {
            NodeInfo info;
            info.host_name = host_name;
            info.room_id = "0";
            _node_list.push_back(info);
        }
        _node_lock.unlock();

        session->close(OK, "");
    } );
}

void post_del_node_handler(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request();

    printf("post_del_node_handler\n");
    int content_length = stoi(request->get_header("Content-Length"));
    session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
    {
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(std::string((const char*)body.data()), root))
        {
            session->close(NOT_ACCEPTABLE, "Data is not acceptable!");
            return;
        }

        std::string host_name;
        if (root["host_name"].isString()) {
            host_name = root["host_name"].asString();
        }
        _node_lock.lock();
        for (std::list<NodeInfo>::iterator it = _node_list.begin(); it != _node_list.end(); it++) {
            if ((*it).host_name == host_name) {
                _node_list.erase(it);
                break;
            }
        }
        _node_lock.unlock();
        session->close(OK, "");
    } );
}

void get_nodelist_handler(const std::shared_ptr< Session > session)
{
    Json::Value root;
    Json::FastWriter writer;
    int i = 0;
    _node_lock.lock();
    for (std::list<NodeInfo>::iterator it = _node_list.begin(); it != _node_list.end(); it++) {
        root[i]["host_name"] = (*it).host_name;
        root[i]["room_id"] = (*it).room_id;
        i++;
    }
    _node_lock.unlock();
    std::string result = writer.write(root);
    session->set_header("Content-Length", std::to_string(result.length()));
    session->close(OK, result);
}

void post_file_handler(const std::shared_ptr< Session > session)
{
	const auto request = session->get_request();

    int content_length = stoi(request->get_header("Content-Length"));
	session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
	{
		const std::string filename = request->get_path_parameter("filename");
		if (access("/var/file", F_OK) != 0) {
            umask(0);
            mkdir("/var/file", S_IRWXU | S_IRWXG | S_IRWXO);
		}
		std::ofstream stream("/var/file/" + filename, std::ofstream::out | std::ofstream::binary);
		stream.write((const char*)body.data(), body.size());
		stream.close();
		session->close(OK, "post file succeed");
	});
}

void get_file_handler(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request( );
    const std::string filename = request->get_path_parameter("filename");
    std::ifstream stream("/var/file/" + filename, std::ifstream::in | std::ifstream::binary);

    if (stream.is_open())
    {
		const std::string body = std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        const std::multimap<std::string, std::string> headers
        {
        	{ "Content-Type", "application/octet-stream" },
            { "Content-Length", std::to_string(body.length()) }
        };
        session->close(OK, body, headers);
        stream.close();
    }
    else
    {
        session->close(NOT_FOUND);
    }
}

void get_filelist_handler(const std::shared_ptr< Session > session)
{
    std::string filename;
    std::vector<std::string> filelist;
    DIR* d = opendir("/var/file");
    if (d != NULL) {
        struct dirent* entry;
        while ((entry = readdir(d)) != NULL) {
            filelist.push_back(std::string(entry->d_name));
        }
        closedir(d);
    }
    Json::Value root;
    Json::FastWriter writer;
    for (unsigned int i = 0; i < filelist.size(); i++) {
        root[i] = filelist[i];
    }
    std::string result = writer.write(root);
    session->set_header("Content-Length", std::to_string(result.length()));
    session->close(OK, result);
}

void get_invite_handler(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request( );
    const std::string invite_name = request->get_query_parameter("invite_name");
    const std::string room_id = request->get_query_parameter("room_id");

    if (invite_name.empty()) {
        Json::Value root;
        Json::FastWriter writer;
        int i = 0;
        for (std::list<InviteInfo>::iterator it = _invite_list.begin(); it != _invite_list.end(); it++) {
            root[i]["invite_name"] = (*it).invite_name;
            root[i]["room_id"] = (*it).room_id;
            i++;
        }
        std::string result = writer.write(root);
        session->set_header("Content-Length", std::to_string(result.length()));
        session->close(OK, result);
    } else {
        printf("invite %s to join\n", invite_name.c_str());
        InviteInfo info;
        info.invite_name = invite_name;
        info.room_id = room_id;
        _invite_list.push_back(info);
        session->close(OK, "");
    }
}

void post_add_room_hanlder(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request();

    int content_length = stoi(request->get_header("Content-Length"));
    session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
    {
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(std::string((const char*)body.data()), root))
        {
			session->close(NOT_ACCEPTABLE, "Data is not acceptable!");
            return;
        }

        std::string room_name;
        std::string host_name;
        std::string room_id;
        int session_port = -1;
        if (root["room_name"].isString()) {
            room_name = root["room_name"].asString();
        }
        if (root["host_name"].isString()) {
            host_name = root["host_name"].asString();
        }
        printf("room_name %s host_name %s\n", room_name.c_str(), host_name.c_str());

        bool found = false;
        pid_t pid = -1;
        _room_lock.lock();
        for (std::list<RoomInfo>::iterator it = _room_list.begin(); it != _room_list.end(); it++) {
            if ((*it).room_name == room_name) {
                found = true;
                room_id = (*it).room_id;
                session_port = (*it).session_port;
                if ((*it).status != 0) {
                    pid = start_session(session_port);
                    if (pid < 0) {
                        _room_lock.unlock();
                        session->close(INTERNAL_SERVER_ERROR, "Can not create session process!");
                        return;
                    }
                    (*it).session_pid = pid;
                    (*it).status = 0;
                }
                break;
            }
        }

        if (!found) {
            RoomInfo info;
            info.room_name = room_name;
            info.room_id = get_room_guid();
            info.session_port = _current_port;
            _current_port++;
            pid = start_session(info.session_port);
            if (pid < 0) {
                _room_lock.unlock();
                session->close(INTERNAL_SERVER_ERROR, "Can not create session process!");
                return;
            }
            info.session_pid = pid;
            info.status = 0;
            _room_list.push_back(info);
            room_id = info.room_id;
            session_port = info.session_port;
        }
        _room_lock.unlock();

        _node_lock.lock();
        for (std::list<NodeInfo>::iterator it = _node_list.begin(); it != _node_list.end(); it++) {
            if ((*it).host_name == host_name) {
                (*it).room_id = room_id;
                break;
            }
        }
        _node_lock.unlock();
        root.clear();
        Json::FastWriter writer;
        root["room_id"] = room_id;
        root["session_port"] = session_port;
        root["reuse"] = found;
        std::string result = writer.write(root);
        session->set_header("Content-Length", std::to_string(result.length()));
        session->close(OK, result);
	});
}

void post_del_room_hanlder(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request();

    int content_length = stoi(request->get_header("Content-Length"));
    session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
    {
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(std::string((const char*)body.data()), root))
        {
            session->close(NOT_ACCEPTABLE, "Data is not acceptable!");
            return;
        }

        std::string room_id;
        if (root["room_id"].isString()) {
            room_id = root["room_id"].asString();
        }

        _room_lock.lock();
        for (std::list<RoomInfo>::iterator it1 = _room_list.begin(); it1 != _room_list.end(); it1++) {
            if ((*it1).room_id == room_id) {
                pid_t pid = (*it1).session_pid;
                if (pid > 0) {
                    printf("kill room session process\n");
                    kill(pid, SIGKILL);
                }
                _room_list.erase(it1);
                break;
            }
        }
        _room_lock.unlock();
        _node_lock.lock();
        for (std::list<NodeInfo>::iterator it2 = _node_list.begin(); it2 != _node_list.end(); it2++) {
            if ((*it2).room_id == room_id) {
                (*it2).room_id = "0";
            }
        }
        _node_lock.unlock();
        session->close(OK, "");
    } );
}

void post_join_room_handler(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request();

    int content_length = stoi(request->get_header("Content-Length"));
    session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
    {
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(std::string((const char*)body.data()), root))
        {
			session->close(NOT_ACCEPTABLE, "Data is not acceptable!");
            return;
        }

        std::string host_name;
        std::string room_id;
        int session_port = -1;
        if (root["host_name"].isString()) {
            host_name = root["host_name"].asString();
        }
        if (root["room_id"].isString()) {
            room_id = root["room_id"].asString();
        }
        printf("host_name %s, room_id %s\n", host_name.c_str(), room_id.c_str());

        _room_lock.lock();
        for (std::list<RoomInfo>::iterator it1 = _room_list.begin(); it1 != _room_list.end(); it1++) {
            if ((*it1).room_id == room_id) {
                if ((*it1).status != 0) {
                    _room_lock.unlock();
                    session->close(INTERNAL_SERVER_ERROR, "Room is abnormal!");
                    return;
                }
                session_port = (*it1).session_port;
                break;
            }
        }
        _room_lock.unlock();
        _node_lock.lock();
        for (std::list<NodeInfo>::iterator it = _node_list.begin(); it != _node_list.end(); it++) {
            if ((*it).host_name == host_name) {
                (*it).room_id = room_id;
                break;
            }
        }
        _node_lock.unlock();

        for (std::list<InviteInfo>::iterator it2 = _invite_list.begin(); it2 != _invite_list.end(); it2++) {
            if ((*it2).invite_name == host_name) {
                _invite_list.erase(it2);
                break;
            }
        }
        root.clear();
        Json::FastWriter writer;
        root["session_port"] = session_port;
        std::string result = writer.write(root);
        session->set_header("Content-Length", std::to_string(result.length()));
        session->close(OK, result);
	});
}

void post_left_room_handler(const std::shared_ptr< Session > session)
{
    const auto request = session->get_request();

    int content_length = stoi(request->get_header("Content-Length"));
    session->fetch(content_length, [request](const std::shared_ptr< Session > session, const Bytes & body)
    {
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(std::string((const char*)body.data()), root))
        {
            session->close(NOT_ACCEPTABLE, "Data is not acceptable!");
            return;
        }

        std::string host_name;
        std::string room_id;
        if (root["host_name"].isString()) {
            host_name = root["host_name"].asString();
        }

        _node_lock.lock();
        for (std::list<NodeInfo>::iterator it = _node_list.begin(); it != _node_list.end(); it++) {
            if ((*it).host_name == host_name) {
                (*it).room_id = "0";
            }
        }
        _node_lock.unlock();
        session->close(OK, "");
    } );
}

int main(int argc, char** argv)
{
	auto resource1 = std::make_shared< Resource >();
	resource1->set_path("/add-node");
	resource1->set_method_handler("POST", post_add_node_handler);

	auto resource2 = std::make_shared< Resource >();
	resource2->set_path("/del-node");
	resource2->set_method_handler("POST", post_del_node_handler);

	auto resource3 = std::make_shared< Resource >();
	resource3->set_path("/nodelist");
	resource3->set_method_handler("GET", get_nodelist_handler);

	auto resource4 = std::make_shared< Resource >();
	resource4->set_path("/file/{filename: .*}");
	resource4->set_method_handler("POST", post_file_handler);
	resource4->set_method_handler("GET", get_file_handler);

	auto resource5 = std::make_shared< Resource >();
	resource5->set_path("/filelist");
	resource5->set_method_handler("GET", get_filelist_handler);

	auto resource6 = std::make_shared< Resource >();
	resource6->set_path("/invite");
	resource6->set_method_handler("GET", get_invite_handler);

	auto resource7 = std::make_shared< Resource >();
	resource7->set_path("/add-room");
	resource7->set_method_handler("POST", post_add_room_hanlder);

	auto resource8 = std::make_shared< Resource >();
	resource8->set_path("/del-room");
	resource8->set_method_handler("POST", post_del_room_hanlder);

	auto resource9 = std::make_shared< Resource >();
	resource9->set_path("/join-room");
	resource9->set_method_handler("POST", post_join_room_handler);

	auto resource10 = std::make_shared< Resource >();
	resource10->set_path("/left-room");
	resource10->set_method_handler("POST", post_left_room_handler);

	auto settings = std::make_shared< Settings >();
	if (argc > 1)
	{
		int port = atoi(argv[1]);
		if (port > 0) {
			settings->set_port(port);
		}
	}
	settings->set_default_header("Connection", "close");

	Service service;
	service.publish(resource1);
	service.publish(resource2);
	service.publish(resource3);
	service.publish(resource4);
	service.publish(resource5);
	service.publish(resource6);
	service.publish(resource7);
	service.publish(resource8);
	service.publish(resource9);
	service.publish(resource10);
	service.start(settings);

    return EXIT_SUCCESS;
}
