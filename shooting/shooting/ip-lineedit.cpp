#include "ip-lineedit.h"
#include <QDebug>

IPLineEdit::IPLineEdit(QWidget *parent) : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Sunken);
    setStyleSheet("QFrame{color:#6B6B6B;border-width:0;border-style:outset}");

    QHBoxLayout* pLayout = new QHBoxLayout(this);
    setLayout(pLayout);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    for (int i = 0; i != QTUTL_IP_SIZE; ++i)
    {
        if (i != 0)
        {
            pDot[i-1] = new QLabel(".", this);
            pDot[i-1]->setStyleSheet("font: bold;font-size: 20px");
            pLayout->addWidget(pDot[i-1]);
            pLayout->setStretch(pLayout->count(), 0);
        }

        m_pLineEdit[i] = new QLineEdit(this);
        m_pLineEdit[i]->setStyleSheet("QLineEdit{border-width:0;border-style:outset}");
        QLineEdit* pEdit = m_pLineEdit[i];
        pEdit->installEventFilter(this);

        pLayout->addWidget(pEdit);
        pLayout->setStretch(pLayout->count(), 1);

        pEdit->setFrame(false);
        pEdit->setAlignment(Qt::AlignCenter);

        QFont font = pEdit->font();
        font.setStyleHint(QFont::Monospace);
        font.setFixedPitch(true);
        font.setPixelSize(20);
        pEdit->setFont(font);

        QRegExp rx ("^(0|[1-9]|[1-9][0-9]|1[0-9][0-9]|2([0-4][0-9]|5[0-5]))$");
        QValidator *validator = new QRegExpValidator(rx, pEdit);
        pEdit->setValidator(validator);
    }

    setFixedSize(40 * QTUTL_IP_SIZE + 15 * (QTUTL_IP_SIZE - 1), 30);
}

IPLineEdit::~IPLineEdit()
{    
    for (int i = 0; i != QTUTL_IP_SIZE; ++i)
    {
        delete m_pLineEdit[i];
    }
    for (int i = 0; i < QTUTL_IP_SIZE - 1; ++i)
    {
        delete pDot[i];
    }
}

bool IPLineEdit::eventFilter(QObject *obj, QEvent *event)
{
    bool bRes = QFrame::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent* pEvent = dynamic_cast<QKeyEvent*>( event );
        if (pEvent)
        {
            for (unsigned int i = 0; i != QTUTL_IP_SIZE; ++i)
            {
                QLineEdit* pEdit = m_pLineEdit[i];
                if (pEdit == obj)
                {
                    switch (pEvent->key())
                    {
                    case Qt::Key_Left:
                        if (pEdit->cursorPosition() == 0)
                        {
                            MovePrevLineEdit(i);
                        }
                        break;

                    case Qt::Key_Right:
                        if (pEdit->text().isEmpty() || (pEdit->text().size() == pEdit->cursorPosition()))
                        {
                            MoveNextLineEdit(i);
                        }
                        break;

                    case Qt::Key_0:
                        if (pEdit->text().isEmpty() || pEdit->text() == "0")
                        {
                            pEdit->setText("0");
                            MoveNextLineEdit(i);
                        }
                        break;

                    case Qt::Key_Backspace:
                        if (pEdit->text().isEmpty() || pEdit->cursorPosition() == 0)
                        {
                            MovePrevLineEdit(i);
                        }
                        break;

                    case Qt::Key_Comma:
                    case Qt::Key_Period:
                        MoveNextLineEdit(i);
                        break;

                    default:
                        break;
                    }
                }
            }
        }
    }

    return bRes;
}

void IPLineEdit::MoveNextLineEdit(unsigned int i)
{
    if (i + 1 != QTUTL_IP_SIZE)
    {
        m_pLineEdit[i+1]->setFocus();
        m_pLineEdit[i+1]->setCursorPosition( 0 );
        m_pLineEdit[i+1]->selectAll();
    }
}

void IPLineEdit::MovePrevLineEdit(unsigned int i)
{
    if (i != 0)
    {
        m_pLineEdit[i-1]->setFocus();
        m_pLineEdit[i-1]->setCursorPosition( m_pLineEdit[i-1]->text().size() );
    }
}


const QString IPLineEdit::ip()
{
    QString ip_str;
    ip_str.clear();
    for (int i = 0;i < QTUTL_IP_SIZE; i++)
    {
        if (i != 0)
            ip_str.append(".");
        if (m_pLineEdit[i]->text().isEmpty())
        {
            return "";
        }

        ip_str.append(m_pLineEdit[i]->text());
    }
    qDebug() << ip_str;
    return ip_str;
}

void IPLineEdit::set_ip(const QString ip)
{
    QStringList ip_list = ip.split(".");
    for (int i = 0; i < ip_list.size(); i++)
    {
        m_pLineEdit[i]->setText(ip_list[i]);
    }
}

void IPLineEdit::start_edit()
{
    m_pLineEdit[0]->setFocus();
}

PortLineEdit::PortLineEdit(QWidget *parent) : QFrame(parent)
{
    setFrameShape( QFrame::StyledPanel );
    setFrameShadow( QFrame::Sunken );

    QHBoxLayout* pLayout = new QHBoxLayout( this );
    setLayout( pLayout );
    pLayout->setContentsMargins( 0, 0, 0, 0 );
    pLayout->setSpacing( 0 );

    QLabel* split = new QLabel(":", this);
    split->setStyleSheet("font: bold;font-size: 20px");
    pLayout->addWidget(split);
    pLayout->setStretch(pLayout->count(), 0);
    m_edit = new QLineEdit(this);
    m_edit->setStyleSheet("QLineEdit{border-width:0;border-style:outset}");
    m_edit->installEventFilter(this);
    m_edit->setMaxLength(5);
    pLayout->addWidget(m_edit);
    pLayout->setStretch(pLayout->count(), 1);
    m_edit->setFrame(false);
    m_edit->setAlignment(Qt::AlignCenter);
    QFont font = m_edit->font();
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPixelSize(20);
    m_edit->setFont(font);
    QRegExp rx ("^([1-9]|[1-9][0-9]|[1-9][0-9][0-9]|[1-9][0-9][0-9][0-9]|[1-5][0-9][0-9][0-9][0-9]|6([0-4][0-9][0-9][0-9]|5([0-4][0-9][0-9]|5([0-2][0-9]|3[0-5]))))$");
    QValidator *validator = new QRegExpValidator(rx, m_edit);
    m_edit->setValidator(validator);

    setFixedSize(80, 30);
    setStyleSheet("QFrame{border-width:0;border-style:outset}");
}

PortLineEdit::~PortLineEdit()
{
    delete m_edit;
}

const QString PortLineEdit::port()
{
    return m_edit->text();
}

void PortLineEdit::set_port(const QString port)
{
    m_edit->setText(port);
}
