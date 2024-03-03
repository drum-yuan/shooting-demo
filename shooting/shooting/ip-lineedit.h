#ifndef IP_LINEEDIT_H
#define IP_LINEEDIT_H

#include <QFrame>
#include <QLineEdit>
#include <QIntValidator>
#include <QHBoxLayout>
#include <QFont>
#include <QLabel>
#include <QKeyEvent>
#include <QHostAddress>

class IPLineEdit : public QFrame
{
    Q_OBJECT

public:
    IPLineEdit(QWidget *parent = nullptr);
    ~IPLineEdit();

    virtual bool eventFilter(QObject *obj, QEvent *event);
    const QString ip();
    void set_ip(const QString ip);
    void start_edit();

private:
    enum
    {
        QTUTL_IP_SIZE   = 4,
        MAX_DIGITS      = 3
    };

    QLineEdit* m_pLineEdit[QTUTL_IP_SIZE];
    QLabel* pDot[QTUTL_IP_SIZE - 1];
    void MoveNextLineEdit (unsigned int i);
    void MovePrevLineEdit (unsigned int i);

};

class PortLineEdit : public QFrame
{
public:
    PortLineEdit(QWidget *parent = nullptr);
    ~PortLineEdit();

    const QString port();
    void set_port(const QString port);
private:
    QLineEdit *m_edit;
};

#endif

