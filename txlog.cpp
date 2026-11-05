#include "txlog.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace TxLog {

bool append(const QString &dir, const QString &filename, const QString &line)
{
    if (!QDir(dir).exists())
        return false;
    QFile f(QDir(dir).filePath(filename));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out << line << '\n';   // Text mode writes CRLF on Windows, like StreamWriter.WriteLine
    out.flush();
    return out.status() == QTextStream::Ok;
}

QString transmissionLine(const QDateTime &startUtc, const QString &callsign, double freqMHz, const QString &locator,
                         int powerdBm, const QString &antenna)
{
    return QString("%1 UTC, %2, %3 MHz, %4, %5 dBm, Antenna: %6")
        .arg(startUtc.toUTC().toString("yyyy-MM-dd HH:mm"), callsign, QString::number(freqMHz, 'g', 10), locator)
        .arg(powerdBm)
        .arg(antenna);
}

bool appendTransmission(const QDateTime &startUtc, const QString &callsign, double freqMHz, const QString &locator,
                        int powerdBm, const QString &antenna)
{
    return append(QDir::homePath(), "WSPR_Log",
                  transmissionLine(startUtc, callsign, freqMHz, locator, powerdBm, antenna));
}

} // namespace TxLog
