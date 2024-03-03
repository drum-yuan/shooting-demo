#include "Utils.h"
#include "Protocol.h"
#include <string.h>

int LEVEL = LOG_LEVEL_ERROR;
ofstream log_file;

void utils_set_log_path(const string& path)
{
    log_file = ofstream(path, ios_base::out | ios_base::app);
}

void utils_set_log_level(const string& level)
{
    if (level == "DEBUG") {
        LEVEL = LOG_LEVEL_DEBUG;
    } else if (level == "INFO") {
        LEVEL = LOG_LEVEL_INFO;
    } else if (level == "WARN") {
        LEVEL = LOG_LEVEL_WARN;
    } else {
        LEVEL = LOG_LEVEL_ERROR;
    }
}

void utils_ws_emit_log(int level, const char *line)
{
    if (level == LLL_ERR){
        LOG_ERROR("%s", line);
    }
    else if (level == LLL_WARN){
        LOG_WARN("%s", line);
    }
    else if (level == LLL_NOTICE){
        LOG_INFO("%s", line);
    }
    else if (level == LLL_INFO){
        LOG_INFO("%s", line);
    }
    else {
        LOG_DEBUG("%s", line);
    }
}

int utils_create_send_buffer(uint8_t* buffer, int buffer_len, int type, uint8_t* payload, int payload_len)
{
	MsgHeader header;
	header.type = htons(type);
	header.length = htonl(payload_len);
	header.major = PROTOCOL_MAJOR_VERSION;
	header.minor = PROTOCOL_MINOR_VERSION;
	
	int len = sizeof(MsgHeader) + payload_len;
	if (len + LWS_PRE <= buffer_len) {
		memcpy(buffer + LWS_PRE, &header, sizeof(MsgHeader));
		if (payload != NULL && payload_len > 0) {
			memcpy(buffer + LWS_PRE + sizeof(MsgHeader), payload, payload_len);
		}
		return len;
	}
	else {
		return 0;
	}
}

void utils_sleep(int msec)
{
#ifdef WIN32
	Sleep(msec);
#else
    usleep(msec * 1000);
#endif	
}
