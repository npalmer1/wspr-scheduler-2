// MySqlStarter: already running, started (a fake "MySQL" that begins listening when asked, after a
// delay), not installed, would not start, started but never answering, and the database on another PC.
// The real Windows service / XAMPP start is replaced by the test's own start function.
#include "dbbackoff.h"
#include "mysqlstarter.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int freePort()
{
    QTcpServer s;
    s.listen(QHostAddress::LocalHost);
    return s.serverPort();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace MySqlStarter;

    CHECK(isLocalHost("127.0.0.1") && isLocalHost("localhost") && isLocalHost("") && !isLocalHost("192.0.2.5"));

    // already running
    {
        QTcpServer up;
        up.listen(QHostAddress::LocalHost);
        CHECK(portAnswers("127.0.0.1", up.serverPort(), 1000));
        bool tried = false;
        const Result r = ensureRunning("localhost", up.serverPort(), 5, [&](QString *, QString *) { tried = true; return true; });
        CHECK(r.outcome == Outcome::AlreadyRunning && !tried && message(r, "WSPR Scheduler").isEmpty());
    }

    // not running; starting it works (it begins answering 1.5 s later, in another thread)
    {
        const int port = freePort();
        CHECK(!portAnswers("127.0.0.1", port, 500));
        QThread th;
        QTcpServer *server = nullptr;
        th.start();
        QElapsedTimer t;
        t.start();
        const Result r = ensureRunning("127.0.0.1", port, 10, [&](QString *how, QString *) {
            *how = "the Windows service MySQL80";
            // the "service": listen from the helper thread after a delay
            QObject *ctx = new QObject;
            ctx->moveToThread(&th);
            QMetaObject::invokeMethod(ctx, [&, port, ctx] {
                QThread::msleep(1500);
                server = new QTcpServer(ctx);
                server->listen(QHostAddress::LocalHost, quint16(port));
            }, Qt::QueuedConnection);
            return true;
        });
        CHECK(r.outcome == Outcome::Started && r.how == "the Windows service MySQL80" && t.elapsed() >= 1400);
        CHECK(message(r, "WSPR Scheduler").isEmpty());
        th.quit();
        th.wait();
    }

    // nothing installed
    {
        const Result r = ensureRunning("127.0.0.1", freePort(), 5, [](QString *, QString *) { return false; });
        CHECK(r.outcome == Outcome::NotInstalled);
        const QString m = message(r, "WSPR Scheduler");
#ifdef Q_OS_WIN
        CHECK(m.startsWith("MySQL doesn't seem to be installed") && m.contains("wspr_mysql_setup.exe") && m.contains("XAMPP")
              && m.endsWith("please restart WSPR Scheduler."));
#else
        CHECK(m.startsWith("MySQL doesn't seem to be installed") && m.contains("sudo apt install mariadb-server")
              && !m.contains("XAMPP") && m.endsWith("please restart WSPR Scheduler."));
#endif
    }

    // found, but it would not start (e.g. needs administrator rights)
    {
        const Result r = ensureRunning("127.0.0.1", freePort(), 5, [](QString *how, QString *detail) {
            *how = "the Windows service MySQL80";
            *detail = "Starting the MySQL80 service needs administrator rights: start it from Services (services.msc), "
                      "or set it to start automatically.";
            return false;
        });
        CHECK(r.outcome == Outcome::StartFailed);
        const QString m = message(r, "WSPR Scheduler");
        CHECK(m.startsWith("MySQL was found (the Windows service MySQL80) but could not be started automatically.")
              && m.contains("administrator rights") && m.contains("port 3306"));
#ifndef Q_OS_WIN
        CHECK(m.contains("sudo systemctl enable --now mariadb"));
#endif
    }

    // started, but nothing ever answers (port taken by something else, or MySQL crashed)
    {
        const int port = freePort();
        QElapsedTimer t;
        t.start();
        const Result r = ensureRunning("127.0.0.1", port, 2, [](QString *how, QString *) { *how = "XAMPP (C:\\xampp)"; return true; });
        CHECK(r.outcome == Outcome::StartFailed && r.detail.contains("didn't answer within 2 seconds")
              && r.detail.contains(QString::number(port)) && t.elapsed() < 6000);
    }

    // the database is on another PC: not ours to start
    {
        bool tried = false;
        const Result r = ensureRunning("192.0.2.5", 3306, 5, [&](QString *, QString *) { tried = true; return true; });
        CHECK(r.outcome == Outcome::NotLocal && !tried && message(r, "WSPR Scheduler").isEmpty());
    }

#ifndef Q_OS_WIN
    // Linux: the real start (no test start function) with stand-in systemctl and pkexec scripts on PATH
    {
        QTemporaryDir bin;
        auto script = [&](const QString &name, const QString &body) {
            QFile f(bin.filePath(name));
            f.open(QIODevice::WriteOnly);
            f.write(("#!/bin/sh\n" + body + "\n").toUtf8());
            f.close();
            f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
        };
        const QByteArray oldPath = qgetenv("PATH");
        qputenv("PATH", bin.path().toUtf8() + ":" + oldPath);
        QFile::remove(bin.filePath("calls"));
        // mysql is installed but stopped; the password is cancelled (pkexec exit 126)
        script("systemctl", "echo \"$@\" >> " + bin.filePath("calls") + "\n"
                            "case \"$*\" in\n"
                            "  'show -p LoadState --value mysql') echo loaded ;;\n"
                            "  show*) echo not-found ;;\n"
                            "  'is-active mysql') echo inactive; exit 3 ;;\n"
                            "esac");
        script("pkexec", "echo \"pkexec $@\" >> " + bin.filePath("calls") + "\nexit 126");
        Result r = ensureRunning("127.0.0.1", freePort(), 2);
        CHECK(r.outcome == Outcome::StartFailed && r.how == "the mysql service" && r.detail.contains("needs your password"));
        QFile calls(bin.filePath("calls"));
        calls.open(QIODevice::ReadOnly);
        CHECK(QString::fromUtf8(calls.readAll()).contains("pkexec systemctl start mysql"));
        calls.close();
        // the password is given: started, then waits for the port (nothing answers here: StartFailed after 2 s)
        script("pkexec", "exit 0");
        r = ensureRunning("127.0.0.1", freePort(), 2);
        CHECK(r.outcome == Outcome::StartFailed && r.detail.contains("didn't answer within 2 seconds"));
        // no mysql / mariadb service at all
        script("systemctl", "echo not-found");
        r = ensureRunning("127.0.0.1", freePort(), 2);
        CHECK(r.outcome == Outcome::NotInstalled && message(r, "WSPR Scheduler").contains("sudo apt install mariadb-server"));
        qputenv("PATH", oldPath);
    }
#endif

    // the connection back-off
    {
        DbBackoff b;
        QString e;
        CHECK(!b.blocked(&e) && !b.isDown());
        b.failed("Can't connect to MySQL server");
        CHECK(b.blocked(&e) && e.startsWith("Can't connect to MySQL server") && b.isDown());
        b.reset();
        CHECK(!b.blocked(&e));
        b.failed("x");
        b.ok();
        CHECK(!b.isDown());
    }

    std::printf(failures ? "mysqlstarter_test: %d FAILED\n" : "mysqlstarter_test: all passed\n", failures);
    return failures ? 1 : 0;
}
