#ifndef GLOBALDATA_H
#define GLOBALDATA_H

#include <QString>

typedef struct {
    QString server_ip;
    QString server_port;
    QString session_url;
    QString room_id;
} GlobalData;

extern GlobalData* g_data;

#endif // GLOBALDATA_H
