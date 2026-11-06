#include "mainwindow.h"
#include "mysqlstarter.h"
#include "slotstore.h"

#include <QLabel>
#include <QThread>
#include <QApplication>
#include <QFont>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("G4GCI");
    QCoreApplication::setApplicationName("WSPR Sked");   // QSettings: db/host, db/user, db/password

    // Slightly smaller text than Qt's default (9 pt on Windows): the window keeps the C# form's fixed
    // control positions, and at 9 pt some button and label text ran past its edges.
    // Change with the setting ui/fontPointSize (e.g. 8.5); 0 = Qt's default.
    {
        const double pt = QSettings().value("ui/fontPointSize", 8.0).toDouble();
        if (pt > 0) {
            QFont f = QApplication::font();
            f.setPointSizeF(pt);
            QApplication::setFont(f);
        }
    }
    // C#: checkMySQLDatabases - MySQL on this PC not running: start it (service or XAMPP) before the
    // window loads its settings, slots and antennas. In the background, with a notice, so nothing freezes.
    MySqlStarter::Result mysql;
    {
        DbSettings d;
        QSettings s;
        const QString host = s.value("db/host", d.host).toString();
        const int port = s.value("db/port", d.port).toInt();
        if (MySqlStarter::isLocalHost(host) && !MySqlStarter::portAnswers(host, port, 1500)) {
            QLabel notice("WSPR Scheduler\n\nMySQL isn't running - starting it ... please wait");
            notice.setWindowTitle("WSPR Scheduler");
            notice.setAlignment(Qt::AlignCenter);
            notice.setMinimumSize(380, 120);
            notice.setStyleSheet("background-color: seashell; padding: 16px;");
            notice.show();
            QThread *t = QThread::create([&] { mysql = MySqlStarter::ensureRunning(host, port, 20); });
            QEventLoop loop;
            QObject::connect(t, &QThread::finished, &loop, &QEventLoop::quit);
            t->start();
            loop.exec();
            delete t;
        } else {
            mysql.outcome = MySqlStarter::Outcome::AlreadyRunning;
        }
    }
    MainWindow w;
    w.setMySqlStart(mysql);
    w.show();
    return app.exec();
}
