#include "Utils.h"
#include "Session.h"
#ifndef WIN32
#include <unistd.h>
#endif

int main(int argc,char* argv[])
{
    if (argc < 2)
    {
        LOG_ERROR("Session start error not enough parameters");
        return 0;
    }
    int port = atoi(argv[1]);

	string logfile_name("realtime-session.log");
#ifndef WIN32
	logfile_name = to_string(getpid()) + logfile_name;
#endif
	utils_set_log_path(logfile_name);
	utils_set_log_level("DEBUG");
    Session* pSession = new Session(port);
    if (pSession && pSession->init())
    {
        pSession->run();
    }
    delete pSession;
    return 0;
}
