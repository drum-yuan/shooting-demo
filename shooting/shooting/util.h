#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QVariant>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QEventLoop>
#include <QTimer>
#include <QLabel>

class AnimateLabel : public QLabel
{
    Q_OBJECT

public:
    explicit AnimateLabel(const QString& pic_path, QWidget *parent = nullptr);
    ~AnimateLabel();

public slots:
    void start();
    void stop();
    void update_animate();

private:
    int m_count;
    QString m_pic_path;
    QTimer m_timer;
    QEventLoop m_eventloop;
};

class Util
{
public:
    Util();

    static int is_command_running(const QString& command);
    static int get_mac_addr_num();
    static QHostAddress get_ip_addr();
    static void http_wait(QNetworkReply* net_reply, int msec, AnimateLabel* animate = nullptr);
    static bool has_install_vc2015();
};

#endif // UTIL_H
