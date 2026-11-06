#pragma once
// After a failed attempt to connect to MySQL, don't try again for a while: every attempt can wait for
// the connect timeout (3 s; on Windows even a refused connection on this PC takes about 2 s), and the
// program makes dozens of database calls, so without this a missing server froze the window for minutes.
//  - shared by every store in the program (they all use the same server): one failure covers them all;
//  - before a real attempt, a quick check (0.3 s at most) that the MySQL port answers at all.
// While "down", calls fail at once with the last error; the first call after the pause tries again.
#include "mysqlstarter.h"

#include <QDateTime>
#include <QString>

class DbBackoff
{
public:
    static constexpr int kPauseSecs = 30;
    static constexpr int kQuickCheckMs = 300;

    // true = go ahead and open the connection now; false = don't (err = why)
    bool mayTry(const QString &host, int port, QString *err)
    {
        if (blocked(err))
            return false;
        if (!MySqlStarter::portAnswers(host, port, kQuickCheckMs)) {
            failed(QString("Can't connect to MySQL server on '%1' (port %2 not answering)")
                       .arg(host.isEmpty() ? QString("127.0.0.1") : host).arg(port > 0 ? port : 3306));
            if (err)
                *err = s_error;
            return false;
        }
        return true;
    }
    bool blocked(QString *err) const
    {
        if (!s_until.isValid() || QDateTime::currentDateTimeUtc() >= s_until)
            return false;
        if (err)
            *err = s_error;
        return true;
    }
    void failed(const QString &err)
    {
        s_error = err;
        s_until = QDateTime::currentDateTimeUtc().addSecs(kPauseSecs);
    }
    void ok() { s_until = QDateTime(); }
    void reset() { s_until = QDateTime(); }   // new settings: try straight away
    bool isDown() const { return blocked(nullptr); }

private:
    static inline QDateTime s_until;   // shared: all stores talk to the same server
    static inline QString s_error;
};
