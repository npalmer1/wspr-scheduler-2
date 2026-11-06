#pragma once
// Starts MySQL at program start if it isn't running (C#: checkMySQLDatabases / IsMySqlRunning /
// CheckXamppBeforeInstall / StartXamppMySQL), so the program then loads normally.
//
// On this PC only (host 127.0.0.1 / localhost), Windows only, in this order:
//  1. a MySQL/MariaDB Windows service that is installed but stopped (MySQL80, MySQL84, MySQL, mysql,
//     MariaDB): "net start" - needs administrator rights, otherwise the reason is reported;
//  2. XAMPP (C:\xampp, Program Files, D:\xampp): mysqld --defaults-file=...\my.ini --standalone, as the
//     XAMPP Control Panel does. No administrator rights needed (C# installed a service instead).
// Then waits up to `waitSecs` for the port to answer. Blocking: run it off the UI thread.
#include <QString>
#include <functional>

namespace MySqlStarter {

enum class Outcome {
    AlreadyRunning,   // the port answers: nothing to do
    Started,          // it wasn't running; now it is
    NotLocal,         // the database is on another PC: not ours to start
    NotInstalled,     // no MySQL service and no XAMPP found on this PC
    StartFailed       // found, but it didn't start (see detail)
};

struct Result
{
    Outcome outcome = Outcome::NotInstalled;
    QString how;      // "service MySQL80" / "XAMPP (C:\xampp)"
    QString detail;   // why it failed, for the message
};

bool isLocalHost(const QString &host);
bool portAnswers(const QString &host, int port, int timeoutMs);
QString xamppBase();   // folder with mysql\bin\mysqld.exe, or empty

// try = how to start it (tests replace the Windows parts); empty = the real thing
Result ensureRunning(const QString &host, int port, int waitSecs = 20,
                     const std::function<bool(QString *how, QString *detail)> &tryStart = {});

// the message for the window when it isn't running afterwards (empty for AlreadyRunning / Started / NotLocal)
QString message(const Result &r, const QString &appName);

} // namespace MySqlStarter
