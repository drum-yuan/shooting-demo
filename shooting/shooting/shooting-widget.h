#ifndef SHOOTINGWIDGET_H
#define SHOOTINGWIDGET_H

#include <QWidget>
#include "config-dialog.h"
#include "shooting-room.h"
#include <QTimer>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class ShootingWidget; }
QT_END_NAMESPACE

class ShootingWidget : public QWidget
{
    Q_OBJECT

public:
    ShootingWidget(QWidget *parent = nullptr);
    ~ShootingWidget();

public slots:
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void button_instant_clicked();
    void button_join_clicked();
    void button_config_clicked();
    void button_close_clicked();

    void add_node();
    void quit_room();
    void invite_monitor();

signals:
    void thread_stop();

private:
    void init_setting();
    void join_room(QString room_id);

    Ui::ShootingWidget *ui;

    QPoint m_start_pos;
    ConfigDialog* m_config_dialog;
    ShootingRoom* m_shooting_room;
    Cdaemon* m_daemon;

    QTimer m_invite_timer;
    QThread* m_invite_thread;
};
#endif // SHOOTINGWIDGET_H
