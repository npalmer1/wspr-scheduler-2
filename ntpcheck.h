#pragma once
// PC clock check against an NTP server, and a Windows clock re-sync - port of
// Form1.SyncSystemClockWithNTP (daily at 03:15 and the "sync clock" button).
//
// Differences from the C# version:
//  - the offset uses all four NTP timestamps, with the fraction of a second (C# compared whole seconds
//    from the server with the time the answer arrived, so it was out by up to a second plus the round trip);
//  - the check has a timeout (C# ReceiveAsync could wait for ever);
//  - a failed re-sync is reported (C#: "System clock synced" whatever w32tm said). w32tm needs
//    Administrator rights, as before.
#include <QDateTime>
#include <QString>
#include <optional>

namespace Ntp {

// Clock offset (server - local) in ms from the four timestamps (ms since the epoch):
// t1 request sent (local), t2 request received (server), t3 answer sent (server), t4 answer received (local)
inline qint64 offsetMs(qint64 t1, qint64 t2, qint64 t3, qint64 t4) { return ((t2 - t1) + (t3 - t4)) / 2; }

// 64-bit NTP timestamp (seconds since 1900 + fraction) at buf[pos] -> ms since 1970
qint64 timestampToMs(const unsigned char *buf, int pos);

struct Result
{
    bool ok = false;
    qint64 offsetMs = 0;      // add this to the PC clock to get the right time
    qint64 roundTripMs = 0;
    QString error;
};

// One SNTP query (blocking, up to timeoutMs). Host "pool.ntp.org" as C#.
Result query(const QString &host = QStringLiteral("pool.ntp.org"), int port = 123, int timeoutMs = 2500);

struct SyncResult
{
    bool attempted = false;   // a re-sync was run
    bool ok = false;          // the clock is now within a second
    QString message;          // for the user
};

// C#: if the PC clock is more than a second out, ask Windows to re-sync it (net start w32time,
// w32tm /resync /force) and check again. On other systems the time service does this; only reported.
SyncResult syncIfNeeded(const QString &host = QStringLiteral("pool.ntp.org"));

} // namespace Ntp
