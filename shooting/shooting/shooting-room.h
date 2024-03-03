#ifndef SHOOTINGROOM_H
#define SHOOTINGROOM_H

#include <QWidget>
#include <QTimer>
#include "daemon.h"

namespace Ui {
class ShootingRoom;
}

class ShootingRoom : public QWidget
{
    Q_OBJECT

public:
    explicit ShootingRoom(QWidget *parent = nullptr);
    ~ShootingRoom();

    void set_daemon(Cdaemon* daemon, bool is_controller);

public slots:
    void resizeEvent(QResizeEvent* size);
    void button_invite_clicked();
    void button_play_clicked();
    void button_pause_clicked();
    void button_quit_clicked();
    void button_assign_clicked();
    void refresh_list();

signals:
    void sig_quit();

private:
    void callback_stop_stream(const std::string& file_name);

    Ui::ShootingRoom *ui;

    Cdaemon* m_daemon;
    QTimer m_refresh_timer;
    bool m_is_controller;
};

#endif // SHOOTINGROOM_H
