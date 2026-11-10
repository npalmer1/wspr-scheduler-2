// MySqlStarter - see mysqlstarter.h.
#include "mysqlstarter.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QTcpSocket>
#include <QStandardPaths>
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

#ifdef Q_OS_WIN
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

#ifndef Q_OS_WIN
// Linux (Ubuntu): the mysql or mariadb systemd service; starting it needs root, so pkexec asks for the
// password in the desktop's own window (the program itself never runs as root)
bool startLinuxService(QString *how, QString *detail)
{
    if (QStandardPaths::findExecutable("systemctl").isEmpty()) {
        *detail = "Start MySQL / MariaDB the way this Linux starts services.";
        return false;
    }
    for (const QString &unit : {QStringLiteral("mysql"), QStringLiteral("mariadb"), QStringLiteral("mysqld")}) {
        int code = 0;
        const QString load = run("systemctl", {"show", "-p", "LoadState", "--value", unit}, &code, 10000);
        if (code != 0 || load.trimmed() != "loaded")
            continue;   // not installed under this name
        *how = "the " + unit + " service";
        if (run("systemctl", {"is-active", unit}, &code, 10000).trimmed() == "active")
            return true;   // running, but the port didn't answer yet: wait for it
        const QString pkexec = QStandardPaths::findExecutable("pkexec");
        if (pkexec.isEmpty()) {
            *detail = "Start it in a terminal with:\n    sudo systemctl start " + unit;
            return false;
        }
        // the password window stays up as long as it takes: allow 2 minutes
        const QString said = run(pkexec, {"systemctl", "start", unit}, &code, 120000);
        if (code == 0)
            return true;
        if (code == 126 || code == 127)
            *detail = "Starting the " + unit + " service needs your password (the request was cancelled or "
                      "refused). Or start it in a terminal with:\n    sudo systemctl start " + unit;
        else
            *detail = "The " + unit + " service would not start: " + said.left(200)
                      + "\nSee:  systemctl status " + unit;
        return false;
    }
    return false;   // no service: not installed
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
    return startLinuxService(how, detail);
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
#ifndef Q_OS_WIN
        return "MySQL doesn't seem to be installed on this PC (no mysql or mariadb service found).\n\n"
               "Install it with:\n    sudo apt install mariadb-server\n"
               "then add the admin user and import the databases (wspr_db.sql) - see the installation "
               "instructions.\n\n"
               "Once you have done this, please restart " + appName + ".";
#endif
        return "MySQL doesn't seem to be installed on this PC (no MySQL service and no XAMPP found).\n\n"
               "You can either install XAMPP and import the databases\n"
               "...see the installation instructions to configure XAMPP.\n\n"
               "OR you can use MySQL Community as an alternative\n"
               "...you can run the 'wspr_mysql_setup.exe' installer to do this.\n\n"
               "Once you have done this, please restart " + appName + ".";
    case Outcome::StartFailed:
#ifndef Q_OS_WIN
        return "MySQL was found (" + r.how + ") but could not be started automatically.\n\n"
               + r.detail + "\n\n"
               "To have it start by itself whenever the PC starts:\n    sudo systemctl enable --now mariadb\n"
               "(mysql instead of mariadb if that is what you use), and then restart " + appName + ".\n\n"
               "Is another instance of MySQL or another app using port 3306?";
#endif
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
