#pragma once
// Port of Logging.cs (Log) plus the log-line format from Form1.WSPR_Transmit.
#include <QDateTime>
#include <QString>

namespace TxLog {

// C#: new Log(filepath, filename, content, OpSystem). Appends one line to dir/filename.
// As C#, nothing is written if dir does not exist; unlike C#, failure is reported (returns false).
// Path separators are handled by Qt, so the OpSystem argument is gone.
bool append(const QString &dir, const QString &filename, const QString &line);

// The WSPR_Log line written after each transmission, e.g.
// "2026-11-01 21:42 UTC, G4GCI, 14.0956 MHz, IO90GU, 23 dBm, Antenna: Windom"
// C# took "now - 1 minute" as the start time; here the actual start time is passed in.
QString transmissionLine(const QDateTime &startUtc, const QString &callsign, double freqMHz,
                         const QString &locator, int powerdBm, const QString &antenna);

// C#: home directory + "WSPR_Log"
bool appendTransmission(const QDateTime &startUtc, const QString &callsign, double freqMHz, const QString &locator,
                        int powerdBm, const QString &antenna);

} // namespace TxLog
