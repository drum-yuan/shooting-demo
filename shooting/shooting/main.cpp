#include "shooting-widget.h"
#include "global-data.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

GlobalData* g_data = nullptr;

void verboseMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString text = QString("");
    static const char* typeStr[] = {"[Debug]", "[Warning]", "[Critical]", "[Fatal]" };

    if (type <= QtFatalMsg)
    {
        QByteArray localMsg = msg.toLocal8Bit();
        QString current_date(QDateTime::currentDateTime().toString("dd-MM-yy HH:mm:ss:zzz"));

        QString message = text.append(typeStr[type]).append(" ").append(current_date).append(": ").append(msg);
        QFile file(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/client.log");
        if (file.size() > 1024 * 1024) {
            file.remove();
        }
        file.open(QIODevice::WriteOnly | QIODevice::Append);
        QTextStream text_stream(&file);
        text_stream << message << "\r\n";
        file.close();
        QFileInfo file_info(file);
        if (file_info.size() > 10 * 1024 * 1024) {
            file.remove();
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("test");
    QCoreApplication::setApplicationName("shooting");
    QDir config_path;
    config_path.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    qInstallMessageHandler(verboseMessageHandler);

    QApplication a(argc, argv);
    g_data = new GlobalData;
    ShootingWidget w;
    w.show();
    return a.exec();
}
