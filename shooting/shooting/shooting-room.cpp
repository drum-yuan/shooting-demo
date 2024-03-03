#include "shooting-room.h"
#include "ui_shooting-room.h"
#include "global-data.h"
#include "util.h"
#include <QRadioButton>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostInfo>

ShootingRoom::ShootingRoom(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ShootingRoom)
{
    setWindowFlags(windowFlags()&~Qt::WindowCloseButtonHint&~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);

    connect(ui->invite, SIGNAL(clicked()), this, SLOT(button_invite_clicked()));
    connect(ui->play, SIGNAL(clicked()), this, SLOT(button_play_clicked()));
    connect(ui->pause, SIGNAL(clicked()), this, SLOT(button_pause_clicked()));
    connect(ui->quit, SIGNAL(clicked()), this, SLOT(button_quit_clicked()));
    connect(ui->assign, SIGNAL(clicked()), this, SLOT(button_assign_clicked()));

    connect(&m_refresh_timer, SIGNAL(timeout()), this, SLOT(refresh_list()));
}

ShootingRoom::~ShootingRoom()
{
    delete ui;
}

void ShootingRoom::set_daemon(Cdaemon* daemon, bool is_controller)
{
    m_daemon = daemon;
    m_daemon->show_stream((void*)ui->video->winId());
    m_daemon->set_stop_stream_callback(std::bind(&ShootingRoom::callback_stop_stream, this, std::placeholders::_1));
    if (!is_controller) {
        ui->usergroup->hide();
        ui->play->hide();
        ui->pause->hide();
        ui->assign->hide();
    } else {
        ui->usergroup->show();
        ui->play->show();
        ui->pause->show();
        ui->assign->show();
    }
    m_is_controller = is_controller;
    m_refresh_timer.start(1000);
}

void ShootingRoom::resizeEvent(QResizeEvent* size)
{
    ui->usergroup->move(width() - 300, 370);
    ui->participantgroup->move(width() - 300, 40);
    ui->video->resize(width() - 342, height() - 80);
}

void ShootingRoom::button_invite_clicked()
{
    for (int i = 0; i < ui->userlist->count(); i++) {
        QListWidgetItem* item = ui->userlist->item(i);
        QRadioButton* radio = (QRadioButton*)ui->userlist->itemWidget(item);
        if (radio->isChecked()) {
            QNetworkRequest request;
            QUrl url = QUrl(QString("http://%1:%2/invite?invite_name=%3&room_id=%4").arg(g_data->server_ip).arg(g_data->server_port).arg(radio->text()).arg(g_data->room_id));
            request.setUrl(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
            QNetworkAccessManager manager;
            qDebug() << "invite url " << url;
            QNetworkReply *net_reply = manager.get(request);

            Util::http_wait(net_reply, 5000);
            break;
        }
    }
}

void ShootingRoom::button_play_clicked()
{
    for (int i = 0; i < ui->participantlist->count(); i++) {
        QListWidgetItem* item = ui->participantlist->item(i);
        QRadioButton* radio = (QRadioButton*)ui->participantlist->itemWidget(item);
        if (radio->isChecked()) {
            qDebug() << "play " << radio->text();
            m_daemon->stop_stream();
            m_daemon->start_stream(radio->text().toStdString());
            break;
        }
    }
}

void ShootingRoom::button_pause_clicked()
{
    m_daemon->stop_stream();
}

void ShootingRoom::button_quit_clicked()
{
    m_refresh_timer.stop();

    QString host_name = QHostInfo::localHostName();
    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/left-room").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QJsonObject jsonObject;
    jsonObject.insert("host_name", host_name);
    QJsonDocument document;
    document.setObject(jsonObject);
    QByteArray json = document.toJson(QJsonDocument::Compact);
    QNetworkReply *net_reply = manager.post(request, json);

    Util::http_wait(net_reply, 5000);

    hide();
    emit sig_quit();
}

void ShootingRoom::button_assign_clicked()
{
    for (int i = 0; i < ui->participantlist->count(); i++) {
        QListWidgetItem* item = ui->participantlist->item(i);
        QRadioButton* radio = (QRadioButton*)ui->participantlist->itemWidget(item);
        if (radio->isChecked()) {
            qDebug() << "assign " << radio->text();
            m_daemon->change_controller(radio->text().toStdString());
            break;
        }
    }
}

void ShootingRoom::refresh_list()
{
    qDebug() << "refresh list";
    QNetworkRequest request;
    request.setUrl(QUrl(QString("http://%1:%2/nodelist").arg(g_data->server_ip).arg(g_data->server_port)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    QNetworkAccessManager manager;
    QNetworkReply *net_reply = manager.get(request);

    Util::http_wait(net_reply, 5000);

    int i = 0;
    int j = 0;
    for (i = 0; i < ui->participantlist->count(); i++) {
        QListWidgetItem* item = ui->participantlist->item(i);
        item->setHidden(true);
    }
    for (i = 0; i < ui->userlist->count(); i++) {
        QListWidgetItem* item = ui->userlist->item(i);
        item->setHidden(true);
    }
    QFont font("MicrosoftYaHei", 10, 75);
    qDebug() << "get nodelist reply " << net_reply->error();
    if (net_reply->error() == QNetworkReply::NoError) {
        QByteArray json = net_reply->readAll();
        qDebug() << "nodelist: " << json;
        QJsonParseError json_error;
        QJsonDocument parse_doucment = QJsonDocument::fromJson(json, &json_error);
        if (json_error.error == QJsonParseError::NoError) {
            QJsonArray array = parse_doucment.array();
            for (i = 0; i < array.size(); i++) {
                QString host_name = array[i].toObject().take("host_name").toString();
                QString room_id = array[i].toObject().take("room_id").toString();
                if (room_id == g_data->room_id) {
                    qDebug() << "participant filter";
                    for (j = 0; j < ui->participantlist->count(); j++) {
                        QListWidgetItem* part_item = ui->participantlist->item(j);
                        QRadioButton* part_radio = (QRadioButton*)ui->participantlist->itemWidget(part_item);
                        qDebug() << "participant radio text " << part_radio->text();
                        if (part_radio->text() == host_name) {
                            part_item->setHidden(false);
                            break;
                        }
                    }
                    if (j == ui->participantlist->count()) {
                        qDebug() << "add participant item";
                        QRadioButton* radio = new QRadioButton(host_name);
                        radio->setFont(font);
                        QListWidgetItem* item = new QListWidgetItem();
                        item->setSizeHint(QSize(250, 40));
                        item->setHidden(false);
                        ui->participantlist->addItem(item);
                        ui->participantlist->setItemWidget(item, radio);
                    }
                } else {
                    qDebug() << "user filter";
                    for (j = 0; j < ui->userlist->count(); j++) {
                        QListWidgetItem* user_item = ui->userlist->item(j);
                        QRadioButton* user_radio = (QRadioButton*)ui->userlist->itemWidget(user_item);
                        if (user_radio->text() == host_name) {
                            user_item->setHidden(false);
                            break;
                        }
                    }
                    if (j == ui->userlist->count()) {
                        QRadioButton* radio = new QRadioButton(host_name);
                        radio->setFont(font);
                        QListWidgetItem* item = new QListWidgetItem();
                        item->setSizeHint(QSize(250, 40));
                        item->setHidden(false);
                        ui->userlist->addItem(item);
                        ui->userlist->setItemWidget(item, radio);
                    }
                }
            }
        }
    }
    qDebug() << "delete participant item " << ui->participantlist->count();
    int del_size = 0;
    int size = ui->participantlist->count();
    for (i = 0; i < size; i++) {
        QListWidgetItem* item = ui->participantlist->item(i - del_size);
        if (item->isHidden()) {
            ui->participantlist->takeItem(i - del_size);
            delete ui->participantlist->itemWidget(item);
            delete item;
            del_size++;
        }
    }
    qDebug() << "delete user item " << ui->userlist->count();
    del_size = 0;
    size = ui->userlist->count();
    for (i = 0; i < size; i++) {
        QListWidgetItem* item = ui->userlist->item(i - del_size);
        if (item->isHidden()) {
            ui->userlist->takeItem(i - del_size);
            delete ui->userlist->itemWidget(item);
            delete item;
            del_size++;
        }
    }

    qDebug() << "participant count " << ui->participantlist->count();
    if (ui->participantlist->count() != 0) {
        QString host_name = QHostInfo::localHostName();
        QString controller = QString::fromStdString(m_daemon->get_controller());
        if (!controller.isEmpty()) {
            if (!m_is_controller) {
                if (controller == host_name) {
                    m_is_controller = true;
                    ui->usergroup->show();
                    ui->play->show();
                    ui->pause->show();
                    ui->assign->show();
                }
            } else {
                if (controller != host_name) {
                    m_is_controller = false;
                    ui->usergroup->hide();
                    ui->play->hide();
                    ui->pause->hide();
                    ui->assign->hide();
                }
            }
        }
        QString publisher = QString::fromStdString(m_daemon->get_publisher());
        if (!publisher.isEmpty()) {
            if (publisher == host_name) {
                ui->video->repaint();
            }
        }
    }
}

void ShootingRoom::callback_stop_stream(const std::string& file_name)
{

}
