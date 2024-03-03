#include "util.h"
#include "global-data.h"
#include <QProcess>
#include <QList>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>

AnimateLabel::AnimateLabel(const QString& pic_path, QWidget *parent) : QLabel(parent)
{
    m_count = 0;
    m_pic_path = pic_path;
    setFixedSize(40, 40);
    setStyleSheet(QString("QLabel{border-image: url(%1/%2.png)}").arg(m_pic_path).arg(m_count));
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update_animate()));
}

AnimateLabel::~AnimateLabel()
{

}

void AnimateLabel::start()
{
    qDebug() << "animate start";
    m_count = 1;
    m_timer.start(200);
    show();
    m_eventloop.exec();
}

void AnimateLabel::stop()
{
    qDebug() << "animate stop";
    hide();
    m_timer.stop();
    m_count = 0;
    m_eventloop.quit();
}

void AnimateLabel::update_animate()
{
    setStyleSheet(QString("QLabel{border-image: url(%1/%2.png)}").arg(m_pic_path).arg(m_count));
    m_count = (m_count + 1 > 5) ? 1 : (m_count + 1);
}


Util::Util()
{

}

int Util::is_command_running(const QString& command)
{
    QProcess tasklist;
#ifdef WIN32
    tasklist.start(QString("tasklist -fi \"imagename eq %1.exe\"").arg(command));
#else
    //para << QString("-ef | grep %1 | grep -v grep").arg(command);
    tasklist.start(QString("pidof %1").arg(command));
#endif
    tasklist.waitForFinished();
    QString result = QString::fromLocal8Bit(tasklist.readAllStandardOutput());
    qDebug() << "find command: " << result;
#ifdef WIN32
    int index = result.indexOf(command);
    if (index >= 0) {
        index = result.indexOf(command, index + command.length());
        if (index >= 0) {
            return 2;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
#else
    int index = result.indexOf('\n');
    if (index >= 0) {
        index = result.indexOf(' ');
        if (index >= 0) {
            return 2;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
#endif
}

int Util::get_mac_addr_num()
{
    QString mac_addr = "";
    QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();

    for (int i = 0; i < nets.count(); i ++) {
        if (nets[i].flags().testFlag(QNetworkInterface::IsUp) && nets[i].flags().testFlag(QNetworkInterface::IsRunning) && !nets[i].flags().testFlag(QNetworkInterface::IsLoopBack)) {
            mac_addr = nets[i].hardwareAddress();
            break;
        }
    }
    QStringList addr_list = mac_addr.toLower().split(':');
    QString mac_vid;
    QString mac_pid;
    if (addr_list.size() == 6) {
        mac_vid = (addr_list[0].toInt(nullptr, 16) > 0x7f) ? QString::number(addr_list[0].toInt(nullptr, 16) - 0x7f, 16) : addr_list[0];
        mac_pid = addr_list[3] + addr_list[4] + addr_list[5];
    }
    //qDebug() << "Mac Address " << (mac_vid + mac_pid);
    return (mac_vid + mac_pid).toInt(nullptr, 16);
}

QHostAddress Util::get_ip_addr()
{
    QList<QNetworkAddressEntry> ip_list;
    QHostAddress ip;
    QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();

    for (int i = 0; i < nets.count(); i ++) {
        if (nets[i].flags().testFlag(QNetworkInterface::IsUp) && nets[i].flags().testFlag(QNetworkInterface::IsRunning) && !nets[i].flags().testFlag(QNetworkInterface::IsLoopBack)) {
            ip_list  = nets[i].addressEntries();
            for (int j = 0; j < ip_list.count(); j++) {
                if (ip_list[j].ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    ip = ip_list[j].ip();
                    break;
                }
            }
            break;
        }
    }
    qDebug() << "ip " << ip.toString();
    return ip;
}

void Util::http_wait(QNetworkReply* net_reply, int msec, AnimateLabel* animate)
{
    if (animate == nullptr) {
        QEventLoop event_loop;
        QObject::connect(net_reply, SIGNAL(finished()), &event_loop, SLOT(quit()));
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, SIGNAL(timeout()), &event_loop, SLOT(quit()));
        timer.start(msec);
        event_loop.exec();
        timer.stop();
    } else {
        QObject::connect(net_reply, SIGNAL(finished()), animate, SLOT(stop()));
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, SIGNAL(timeout()), animate, SLOT(stop()));
        timer.start(msec);
        animate->start();
        timer.stop();
    }
}


bool Util::has_install_vc2015()
{
    QString header = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
    QSettings reg(header,QSettings::NativeFormat);
    QStringList sum = reg.allKeys();
    for (int m  = 0; m < sum.size(); ++m) {
        QString id = sum.at(m);
        int end = id.indexOf("}");
        if (end > 0) {
            id = id.mid(0, end+1);

            QSettings gt(header + id, QSettings::NativeFormat);
            QString name = gt.value("DisplayName").toString();
            if (!name.isEmpty() && (name.contains("Microsoft Visual C++ 2015 Redistributable (x86)") ||
                                    name.contains("Microsoft Visual C++ 2017 Redistributable (x86)") ||
                                    name.contains("Microsoft Visual C++ 2019 Redistributable (x86)") ||
                                    name.contains("Microsoft Visual C++ 2015-2019 Redistributable (x86)"))) {
                return true;
            }
        }
    }
    return false;
}




