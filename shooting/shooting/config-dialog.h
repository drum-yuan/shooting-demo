#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>
#include "ip-lineedit.h"

namespace Ui {
class ConfigDialog;
}

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget *parent = nullptr);
    ~ConfigDialog();

public slots:
    void keyPressEvent(QKeyEvent *event);
    void button_done_clicked();

signals:
    void sig_done();

private:
    Ui::ConfigDialog *ui;

    IPLineEdit* m_ipaddr_line;
    PortLineEdit* m_port_line;
};

#endif // CONFIGDIALOG_H
