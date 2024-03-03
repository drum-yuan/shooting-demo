#ifndef __UTILS_H__
#define __UTILS_H__

#include <memory>
#include <fstream>
#include <string>
#include "libwebsockets.h"

using namespace std;

#define PROTOCOL_MAJOR_VERSION 1
#define PROTOCOL_MINOR_VERSION 1

#define LOG_LEVEL_ERROR 	0
#define LOG_LEVEL_WARN 		1
#define LOG_LEVEL_INFO 		2
#define LOG_LEVEL_DEBUG		3

#define WEBSOCKET_MAX_BUFFER_SIZE   (1024*1024)
#define COMMAND_BUFFER_SIZE		(1024)
#define CONNECT_TIMEOUT 5000

enum ClientConnState {
	ConnectStateUnConnected = 0,
	ConnectStateConnecting = 1,
	ConnectStateConnectError = 2,
	ConnectStateEstablished = 3,
	ConnectStateClosed = 4
};

extern int LEVEL;
extern ofstream log_file;

#define LOG_DEBUG(...)			if(LEVEL >= LOG_LEVEL_DEBUG){ char buf[1024]; snprintf(buf, 1024, ##__VA_ARGS__); log_file << buf << std::endl; }
#ifdef WIN32
#define LOG_INFO(...)			if(LEVEL >= LOG_LEVEL_INFO){ char buf[1024]; char time[64]; SYSTEMTIME sys;\
									snprintf(buf, 1024, ##__VA_ARGS__); GetLocalTime(&sys); snprintf(time, 64, "[INFO](%02d:%02d:%02d.%03d)", sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds); log_file << time << buf << std::endl; }

#define LOG_WARN(...)			if(LEVEL >= LOG_LEVEL_WARN){ char buf[1024]; char time[64]; SYSTEMTIME sys;\
									snprintf(buf, 1024, ##__VA_ARGS__); GetLocalTime(&sys); snprintf(time, 64, "[WARN](%02d:%02d:%02d.%03d)", sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds); log_file << time << buf << std::endl; }

#define LOG_ERROR(...)			if(LEVEL >= LOG_LEVEL_ERROR){ char buf[1024]; char time[64]; SYSTEMTIME sys;\
									snprintf(buf, 1024, ##__VA_ARGS__); GetLocalTime(&sys); snprintf(time, 64, "[ERROR](%02d:%02d:%02d.%03d)", sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds); log_file << time << buf << std::endl; }
#else
#define LOG_INFO(...)			if(LEVEL >= LOG_LEVEL_INFO){ char buf[1024]; char time[64]; struct timeval tv; struct timezone tz; struct tm *p;\
									snprintf(buf, 1024, ##__VA_ARGS__); gettimeofday(&tv, &tz); p = localtime(&tv.tv_sec); snprintf(time, 64, "[INFO](%02d:%02d:%02d.%03ld)", p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec); log_file << time << buf << std::endl; }

#define LOG_WARN(...)			if(LEVEL >= LOG_LEVEL_WARN){ char buf[1024]; char time[64]; struct timeval tv; struct timezone tz; struct tm *p;\
									snprintf(buf, 1024, ##__VA_ARGS__); gettimeofday(&tv, &tz); p = localtime(&tv.tv_sec); snprintf(time, 64, "[INFO](%02d:%02d:%02d.%03ld)", p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec); log_file << time << buf << std::endl; }

#define LOG_ERROR(...)			if(LEVEL >= LOG_LEVEL_ERROR){ char buf[1024]; char time[64]; struct timeval tv; struct timezone tz; struct tm *p;\
									snprintf(buf, 1024, ##__VA_ARGS__); gettimeofday(&tv, &tz); p = localtime(&tv.tv_sec); snprintf(time, 64, "[INFO](%02d:%02d:%02d.%03ld)", p->tm_hour, p->tm_min, p->tm_sec, tv.tv_usec); log_file << time << buf << std::endl; }
#endif

void utils_set_log_path(const string& path);

void utils_set_log_level(const string& level);

void utils_ws_emit_log(int level, const char *line);

int utils_create_send_buffer(uint8_t* buffer, int buffer_len, int type, uint8_t* payload, int payload_len);

void utils_sleep(int msec);


#define MAX_VIDEO_NUM       5

#endif // __UTILS_H__
