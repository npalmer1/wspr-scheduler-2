#pragma once
// Serial (COM port) antenna switch W-410A - port of W410A.cs (W410A_Switch) plus Form1's
// W410A_setAnt / W410A_setMode. Commands are sent as one line:
//   "W410A0+P<n>"  select antenna port n        "W410A0+M<m>"  set mode m
//
// Differences from the C# version:
//  - failures are reported. The C# class put any exception text (e.g. "The port 'COM3' does not exist")
//    into reply, and Form1 set ok = true for any reply, so a missing or unplugged switch was treated as
//    switched and the TX went ahead on the previous antenna;
//  - a reply ending in CR, LF or CR+LF is accepted (C# ReadLine waited for LF only and timed out otherwise);
//  - the serial settings string "8,none,1,none" is validated instead of silently defaulting.
// Needs the Qt Serial Port module (Qt Maintenance Tool -> add "Qt Serial Port").
#include <QString>
#include <optional>

namespace W410A {

struct SerialSettings
{
    QString port;              // "COM3", "/dev/ttyUSB0"
    int baud = 9600;
    int dataBits = 8;
    QString parity = "none";   // none | even | odd
    int stopBits = 1;          // 1 | 2
    QString flow = "none";     // none | xon/xoff | RTS   (as stored by the C# app)
};

// The C# app stores data,parity,stop,flow as "8,none,1,none" (SW[].Serial)
std::optional<SerialSettings> parseSettings(const QString &port, const QString &baud, const QString &spec,
                                            QString *error = nullptr);

struct Result
{
    bool ok = false;
    QString reply;
    QString error;
};

Result send(const SerialSettings &s, const QString &message, int timeoutMs = 1500);
inline Result setAntenna(const SerialSettings &s, int antNo) { return send(s, QString("W410A0+P%1").arg(antNo)); }
inline Result setMode(const SerialSettings &s, int mode) { return send(s, QString("W410A0+M%1").arg(mode)); }

} // namespace W410A
