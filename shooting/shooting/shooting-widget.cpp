#include "shooting-widget.h"
#include "ui_shooting-widget.h"
#include "global-data.h"
#include "util.h"
#include <QStandardPaths>
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostInfo>

ShootingWidget::ShootingWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShootingWidget)
{
    setWindowFlags(Qt::FramelessWindowHint);
    ui->setupUi(this);

    ui->instant_btn->setStyleSheet("background-color: white");
    ui->join_btn->setStyleSheet("background-color: white");
    connect(ui->instant_btn, SIGNAL(clicked()), this, SLOT(button_instant_clicked()));
    connect(ui->join_btn, SIGNAL(clicked()), this, SLOT(button_join_clicked()));
    connect(ui->config_btn, SIGNAL(clicked()), this, SLOT(button_config_clicked()));
    connect(ui->close_btn, SIGNAL(clicked()), this, SLOT(button_close_clicked()));

    init_setting();
    m_config_dialog = new ConfigDialog(this);
    m_config_dialog->hide();
    m_shooting_room = new ShootingRoom();
    m_shooting_room->hide();

    connect(m_config_dialog, SIGNAL(sig_done()), this, SLOT(add_node()));
    connect(m_shooting_room, SIGNAL(sig_quit()), this, SLOT(quit_room()));

    if (g_data->server_ip.isEmpty() || g_data->server_port.isEmpty()) {
        m_config_dialog->show();
    } else {
        add_node();
    }

    m_invite_thread = new QThread(this);
    m_invite_timer.start(3000);
    m_invite_timer.moveToThread(m_invite_thread);
    connect(&m_invite_timer, SIGNAL(timeout()), this, SLOT(invite_monitor()), Qt::DirectConnection);
    connect(this, SIGNAL(thread_stop()), &m_invite_timer, SLOT(stop()), Qt::BlockingQueuedConnection);
    m_invite_thread->start();
}

ShootingWidget::~ShootingWidget()
{
    qDebug() << "app exit!";
    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/del-node").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QJsonObject jsonObject;
    jsonObject.insert("host_name", QHostInfo::localHostName());
    QJsonDocument document;
    document.setObject(jsonObject);
    QByteArray json = document.toJson(QJsonDocument::Compact);
    QNetworkReply *net_reply = manager.post(request, json);

    Util::http_wait(net_reply, 5000);

    emit thread_stop();
    m_invite_thread->quit();
    m_invite_thread->wait();
    delete m_invite_thread;

    delete m_shooting_room;
    delete m_config_dialog;
    delete ui;
}

void ShootingWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!isVisible()) {
        return;
    }
    if (event->buttons() == Qt::LeftButton) {
        if (m_start_pos != QPoint(-1, -1)) {
            QPoint end_pos = event->globalPos() - m_start_pos;
            move(end_pos);
        }
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void ShootingWidget::mousePressEvent(QMouseEvent *event)
{
    if (!isVisible()) {
        return;
    }
    if (event->buttons() == Qt::LeftButton) {
        qDebug() << "set start pos";
        m_start_pos = event->globalPos() - frameGeometry().topLeft();
    }

    QWidget::mousePressEvent(event);
}

void ShootingWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!isVisible()) {
        return;
    }
    m_start_pos = QPoint(-1, -1);

    QWidget::mouseReleaseEvent(event);
}

void ShootingWidget::button_instant_clicked()
{
    QString host_name = QHostInfo::localHostName();
    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/add-room").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QJsonObject jsonObject;
    jsonObject.insert("host_name", host_name);
    jsonObject.insert("room_name", ui->name->text());
    QJsonDocument document;
    document.setObject(jsonObject);
    QByteArray json = document.toJson(QJsonDocument::Compact);
    QNetworkReply *net_reply = manager.post(request, json);

    Util::http_wait(net_reply, 5000);

    if (net_reply->error() == QNetworkReply::NoError) {
        QByteArray json = net_reply->readAll();
        QJsonParseError json_error;
        QJsonDocument parse_doucment = QJsonDocument::fromJson(json, &json_error);
        if (json_error.error == QJsonParseError::NoError) {
            QJsonObject obj = parse_doucment.object();
            g_data->room_id = obj.take("room_id").toString();
            int session_port = obj.take("session_port").toInt();
            bool reuse = obj.take("reuse").toBool();
            g_data->session_url = QString("%1:%2").arg(g_data->server_ip).arg(session_port);
            m_daemon = new Cdaemon(g_data->session_url.toStdString(), host_name.toStdString(), !reuse);
            hide();
            m_shooting_room->set_daemon(m_daemon, true);
            m_shooting_room->show();
        }
    }
}

void ShootingWidget::button_join_clicked()
{
    if (ui->name->text().isEmpty()) {
        if (!g_data->room_id.isEmpty()) {
            join_room(g_data->room_id);
            ui->join_btn->setStyleSheet("background-color: white");
        }
    } else {
        join_room(ui->name->text());
    }
}

void ShootingWidget::button_config_clicked()
{
    m_config_dialog->show();
}

void ShootingWidget::button_close_clicked()
{
    close();
}

void ShootingWidget::add_node()
{
    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/add-node").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QJsonObject jsonObject;
    jsonObject.insert("host_name", QHostInfo::localHostName());
    QJsonDocument document;
    document.setObject(jsonObject);
    QByteArray json = document.toJson(QJsonDocument::Compact);
    QNetworkReply *net_reply = manager.post(request, json);

    Util::http_wait(net_reply, 5000);

    if (net_reply->error() != QNetworkReply::NoError) {
        qWarning() << "Connect server fail";
    }
}

void ShootingWidget::quit_room()
{
    delete m_daemon;
    g_data->room_id.clear();
    g_data->session_url.clear();
    show();
}

void ShootingWidget::invite_monitor()
{
    if (!g_data->session_url.isEmpty()) {
        return;
    }

    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/invite").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QNetworkReply *net_reply = manager.get(request);

    Util::http_wait(net_reply, 5000);

    if (net_reply->error() == QNetworkReply::NoError) {
        QByteArray json = net_reply->readAll();
        qDebug() << "invite list " << json;
        QJsonParseError json_error;
        QJsonDocument parse_doucment = QJsonDocument::fromJson(json, &json_error);
        if (json_error.error == QJsonParseError::NoError) {
            QString host_name = QHostInfo::localHostName();
            QJsonArray array = parse_doucment.array();
            for (int i = 0; i < array.size(); i++) {
                if (host_name == array[i].toObject().take("invite_name").toString()) {
                    g_data->room_id = array[i].toObject().take("room_id").toString();
                    ui->join_btn->setStyleSheet("background-color: green");
                    break;
                }
            }
        }
    }
}

void ShootingWidget::init_setting()
{
    QString config_path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QSettings setting(config_path + "/setting.ini", QSettings::IniFormat);
    setting.beginGroup("setting");
    g_data->server_ip = setting.value("server-ip").toString();
    g_data->server_port = setting.value("server-port").toString();
    setting.endGroup();
}

void ShootingWidget::join_room(QString room_id)
{
    qDebug() << "join room " << room_id;
    QString host_name = QHostInfo::localHostName();
    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/join-room").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QJsonObject jsonObject;
    jsonObject.insert("host_name", host_name);
    jsonObject.insert("room_id", room_id);
    QJsonDocument document;
    document.setObject(jsonObject);
    QByteArray json = document.toJson(QJsonDocument::Compact);
    QNetworkReply *net_reply = manager.post(request, json);

    Util::http_wait(net_reply, 5000);

    if (net_reply->error() == QNetworkReply::NoError) {
        QByteArray json = net_reply->readAll();
        QJsonParseError json_error;
        QJsonDocument parse_doucment = QJsonDocument::fromJson(json, &json_error);
        if (json_error.error == QJsonParseError::NoError) {
            QJsonObject obj = parse_doucment.object();
            int session_port = obj.take("session_port").toInt();
            g_data->session_url = QString("%1:%2").arg(g_data->server_ip).arg(session_port);
            g_data->room_id = room_id;
            m_daemon = new Cdaemon(g_data->session_url.toStdString(), host_name.toStdString(), false);
            hide();
            m_shooting_room->set_daemon(m_daemon, false);
            m_shooting_room->show();
        }
    }
}
