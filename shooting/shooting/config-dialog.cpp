#include "config-dialog.h"
#include "ui_configdialog.h"
#include "global-data.h"
#include "util.h"
#include <QStandardPaths>
#include <QSettings>
#include <QProcess>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostInfo>

ConfigDialog::ConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);

    m_ipaddr_line = new IPLineEdit(this);
    m_port_line = new PortLineEdit(this);
    m_ipaddr_line->move(140, 90);
    m_port_line->move(m_ipaddr_line->pos().rx() + m_ipaddr_line->width() + 5, m_ipaddr_line->pos().ry());

    connect(ui->done_btn, SIGNAL(clicked()), this, SLOT(button_done_clicked()));

    m_ipaddr_line->set_ip(g_data->server_ip);
    m_port_line->set_port(g_data->server_port);
}

ConfigDialog::~ConfigDialog()
{
    delete m_ipaddr_line;
    delete m_port_line;
    delete ui;
}

void ConfigDialog::keyPressEvent(QKeyEvent *event)
{
    if (!isVisible()) {
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        hide();
    } else if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        button_done_clicked();
    }
}

void ConfigDialog::button_done_clicked()
{
    QString ipaddr = m_ipaddr_line->ip();
    QString port = m_port_line->port();
    if (ipaddr.isEmpty()) {
        return;
    }
    if (port.isEmpty()) {
        m_port_line->set_port("80");
        port = "80";
    }

    QString config_path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QSettings setting(config_path + "/setting.ini", QSettings::IniFormat);
    setting.beginGroup("setting");
    bool sync = false;
    if (setting.value("server-ip").toString() != ipaddr) {
        g_data->server_ip = ipaddr;
        setting.setValue("server-ip", QVariant(ipaddr));
        sync = true;
    }
    if (setting.value("server-port").toString() != port) {
        g_data->server_port = port;
        setting.setValue("server-port", QVariant(port));
        sync = true;
    }
    setting.endGroup();
    if (sync) {
        QProcess::execute("sync");
        qDebug() << "save setup server-ip, sync...";
        emit sig_done();
    }
    hide();
}
