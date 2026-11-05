#include "ntpcheck.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHostInfo>
#include <QProcess>
#include <QThread>
#include <QUdpSocket>
#include <cstring>

namespace Ntp {

namespace {
constexpr qint64 kNtpToUnixSeconds = 2208988800LL;   // 1900-01-01 to 1970-01-01

void putTimestamp(unsigned char *buf, int pos, qint64 msSince1970)
{
    const quint64 secs = quint64(msSince1970 / 1000 + kNtpToUnixSeconds);
    const quint64 frac = quint64((msSince1970 % 1000) * 4294967296.0 / 1000.0);
    for (int i = 0; i < 4; ++i) {
        buf[pos + i] = uchar(secs >> (24 - 8 * i));
        buf[pos + 4 + i] = uchar(frac >> (24 - 8 * i));
    }
}
} // namespace

qint64 timestampToMs(const unsigned char *buf, int pos)
{
    quint64 secs = 0, frac = 0;
    for (int i = 0; i < 4; ++i) {
        secs = (secs << 8) | buf[pos + i];
        frac = (frac << 8) | buf[pos + 4 + i];
    }
    return (qint64(secs) - kNtpToUnixSeconds) * 1000 + qint64(frac * 1000.0 / 4294967296.0);
}

Result query(const QString &host, int port, int timeoutMs)
{
    Result r;
    QHostAddress addr(host);
    if (addr.isNull()) {
        const QHostInfo info = QHostInfo::fromName(host);
        for (const QHostAddress &a : info.addresses())
            if (a.protocol() == QAbstractSocket::IPv4Protocol) { addr = a; break; }
        if (addr.isNull() && !info.addresses().isEmpty())
            addr = info.addresses().first();
    }
    if (addr.isNull()) {
        r.error = "cannot find " + host;
        return r;
    }
    QUdpSocket s;
    unsigned char pkt[48] = {};
    pkt[0] = 0x1B;   // LI 0, version 3, client mode (as C#)
    const qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    putTimestamp(pkt, 40, t1);   // transmit timestamp, echoed back as "originate"
    if (s.writeDatagram(reinterpret_cast<const char *>(pkt), 48, addr, quint16(port)) != 48) {
        r.error = s.errorString();
        return r;
    }
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (!s.waitForReadyRead(int(qMax<qint64>(1, timeoutMs - t.elapsed()))))
            break;
        while (s.hasPendingDatagrams()) {
            unsigned char ans[128];
            const qint64 n = s.readDatagram(reinterpret_cast<char *>(ans), sizeof ans);
            const qint64 t4 = QDateTime::currentMSecsSinceEpoch();
            if (n < 48 || (ans[0] & 0x07) != 4)   // not a server answer
                continue;
            if ((ans[1] == 0) || timestampToMs(ans, 40) <= 0) {
                r.error = "the time server did not give the time (kiss-of-death)";
                return r;
            }
            const qint64 t2 = timestampToMs(ans, 32), t3 = timestampToMs(ans, 40);
            r.ok = true;
            r.offsetMs = offsetMs(t1, t2, t3, t4);
            r.roundTripMs = (t4 - t1) - (t3 - t2);
            return r;
        }
    }
    r.error = QString("no answer from %1 (UDP port %2)").arg(host).arg(port);
    return r;
}

SyncResult syncIfNeeded(const QString &host)
{
    SyncResult out;
    const Result before = query(host);
    if (!before.ok) {
        out.message = "Clock not checked: " + before.error;
        return out;
    }
    const auto secs = [](qint64 ms) { return QString::number(ms / 1000.0, 'f', 2); };
    if (qAbs(before.offsetMs) <= 1000) {   // C#: re-sync only when more than a second out
        out.ok = true;
        out.message = QString("PC clock OK (%1 s from %2)").arg(secs(before.offsetMs), host);
        return out;
    }
#ifdef Q_OS_WIN
    out.attempted = true;
    QProcess::execute("net", {"start", "w32time"});   // already running is fine
    QProcess p;
    p.start("w32tm", {"/resync", "/force"});
    p.waitForFinished(20000);
    const QString said = QString::fromLocal8Bit(p.readAllStandardOutput() + p.readAllStandardError()).simplified();
    QThread::msleep(1000);
    const Result after = query(host);
    if (after.ok && qAbs(after.offsetMs) <= 1000) {
        out.ok = true;
        out.message = QString("System clock synced (was %1 s off, now %2 s)").arg(secs(before.offsetMs), secs(after.offsetMs));
    } else {
        out.message = QString("PC clock is %1 s off and could not be re-synced%2. Run WSPR Sked as Administrator "
                              "(w32tm needs it).")
                          .arg(secs(before.offsetMs))
                          .arg(said.isEmpty() ? QString() : " (w32tm: " + said.left(160) + ")");
    }
#else
    out.message = QString("PC clock is %1 s off - check the system time service (timedatectl / chrony)")
                      .arg(secs(before.offsetMs));
#endif
    return out;
}

} // namespace Ntp
