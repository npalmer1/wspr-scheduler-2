// MySqlStarter - see mysqlstarter.h.
#include "mysqlstarter.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QTcpSocket>
#include <QThread>

namespace MySqlStarter {

bool isLocalHost(const QString &host)
{
    const QString h = host.trimmed().toLower();
    return h.isEmpty() || h == "localhost" || h == "127.0.0.1" || h == "::1";
}

bool portAnswers(const QString &host, int port, int timeoutMs)
{
    QTcpSocket s;
    s.connectToHost(isLocalHost(host) ? QString("127.0.0.1") : host, quint16(port > 0 ? port : 3306));
    const bool ok = s.waitForConnected(timeoutMs);
    s.abort();
    return ok;
}

QString xamppBase()
{
    for (const QString &d : {QStringLiteral("C:/xampp"), QStringLiteral("C:/Program Files/xampp"),
                             QStringLiteral("C:/Program Files (x86)/xampp"), QStringLiteral("D:/xampp")})
        if (QFile::exists(d + "/mysql/bin/mysqld.exe"))
            return d;
    return QString();
}

namespace {
#ifdef Q_OS_WIN
QString run(const QString &program, const QStringList &args, int *exitCode, int timeoutMs = 20000)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForStarted(5000)) {
        *exitCode = -1;
        return program + " would not start";
    }
    if (!p.waitForFinished(timeoutMs))
        p.kill();
    *exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    return QString::fromLocal8Bit(p.readAll()).trimmed();
}

// installed MySQL/MariaDB service that isn't running -> "net start"
bool startService(QString *how, QString *detail, bool *found)
{
    for (const QString &name : {QStringLiteral("MySQL80"), QStringLiteral("MySQL84"), QStringLiteral("MySQL"),
                                QStringLiteral("mysql"), QStringLiteral("MariaDB")}) {
        int code = 0;
        const QString out = run("sc", {"query", name}, &code);
        if (code != 0 || !out.contains("STATE"))
            continue;   // not installed (1060)
        *found = true;
        *how = "the Windows service " + name;
        if (out.contains("RUNNING"))
            return true;   // running, but the port didn't answer yet: wait for it
        const QString started = run("net", {"start", name}, &code, 60000);
        if (code == 0)
            return true;
        if (started.contains("System error 5") || started.contains("Access is denied", Qt::CaseInsensitive))
            *detail = "Starting the " + name + " service needs administrator rights: start it from Services "
                      "(services.msc), or set it to start automatically.";
        else
            *detail = "The " + name + " service would not start: " + started.left(200);
        return false;
    }
    return false;
}

bool startXampp(QString *how, QString *detail, bool *found)
{
    const QString base = xamppBase();
    if (base.isEmpty())
        return false;
    *found = true;
    *how = "XAMPP (" + QDir::toNativeSeparators(base) + ")";
    // as XAMPP's mysql_start.bat / Control Panel: runs on its own, stays running after we exit
    const QString mysqld = QDir::toNativeSeparators(base + "/mysql/bin/mysqld.exe");
    const QString ini = QDir::toNativeSeparators(base + "/mysql/bin/my.ini");
    if (!QProcess::startDetached(mysqld, {"--defaults-file=" + ini, "--standalone"}, QDir::toNativeSeparators(base))) {
        *detail = "XAMPP's MySQL (" + mysqld + ") would not start.";
        return false;
    }
    return true;
}
#endif

bool realStart(QString *how, QString *detail)
{
#ifdef Q_OS_WIN
    bool found = false;
    if (startService(how, detail, &found))
        return true;
    const QString serviceProblem = *detail;
    if (startXampp(how, detail, &found))
        return true;
    if (!serviceProblem.isEmpty() && detail->isEmpty())
        *detail = serviceProblem;
    if (!found)
        how->clear();
    return false;
#else
    Q_UNUSED(how);
    *detail = "Start the MySQL / MariaDB service (e.g. sudo systemctl start mariadb).";
    return false;
#endif
}
} // namespace

Result ensureRunning(const QString &host, int port, int waitSecs, const std::function<bool(QString *, QString *)> &tryStart)
{
    Result r;
    if (portAnswers(host, port, 1500)) {
        r.outcome = Outcome::AlreadyRunning;
        return r;
    }
    if (!isLocalHost(host)) {
        r.outcome = Outcome::NotLocal;
        return r;
    }
    const bool started = tryStart ? tryStart(&r.how, &r.detail) : realStart(&r.how, &r.detail);
    if (!started) {
        r.outcome = r.how.isEmpty() ? Outcome::NotInstalled : Outcome::StartFailed;
        return r;
    }
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < qint64(waitSecs) * 1000) {
        if (portAnswers(host, port, 1000)) {
            r.outcome = Outcome::Started;
            return r;
        }
        QThread::msleep(500);
    }
    r.outcome = Outcome::StartFailed;
    r.detail = "It was started (" + r.how + ") but didn't answer within " + QString::number(waitSecs)
               + " seconds. Is another program already using port " + QString::number(port > 0 ? port : 3306) + "?";
    return r;
}

QString message(const Result &r, const QString &appName)
{
    switch (r.outcome) {
    case Outcome::NotInstalled:
        return "MySQL doesn't seem to be installed on this PC (no MySQL service and no XAMPP found).\n\n"
               "You can either install XAMPP and import the databases\n"
               "...see the installation instructions to configure XAMPP.\n\n"
               "OR you can use MySQL Community as an alternative\n"
               "...you can run the 'wspr_mysql_setup.exe' installer to do this.\n\n"
               "Once you have done this, please restart " + appName + ".";
    case Outcome::StartFailed:
        return "MySQL was found (" + r.how + ") but could not be started automatically.\n\n"
               + r.detail + "\n\n"
               "Please start MySQL - with the XAMPP Control Panel, or Services for MySQL Community - "
               "and then restart " + appName + ".\n\n"
               "Is another instance of MySQL or another app using port 3306?";
    default:
        return QString();
    }
}

} // namespace MySqlStarter
