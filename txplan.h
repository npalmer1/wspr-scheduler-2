#pragma once
// Transmit scheduling and hardware selection, without the window - port of the timing in
// Form1.WSPRtimer_Action / daytimer_Action and of activateAntSwitch / changeAntenna /
// Arduino_Comms_1 / W410A_setAnt / changeFilter / Arduino_Comms_Filter.
//
// Timeline of one 2-minute cycle (UTC, as C#):
//   odd  :45      transmitting: block RX decodes until even :15   (C#: RXblocktimer)
//   odd  :54-57   look up the slot for the next even minute          (C#: findSlot)
//   odd  :55      (5 s before) switch antenna, filter, rig frequency (C#: WSPRtimer prep)
//   even :00-02   if the slot is active and TX is enabled: PTT on, audio from :01 (C#: StartTX)
//   about odd :52 audio ends (110.6 s), PTT off                      (C#: wsprTXtimer)
//
// Differences from the C# version:
//  - a switch that does not answer stops the transmission (C# went ahead on the previous antenna -
//    see arduinoswitch.h / w410a.h); a missing antenna name falls back to the default antenna, as C#;
//  - a switch type the program cannot drive ("Manual", etc.) is reported, not silently skipped.
#include "slotstore.h"

#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>

namespace TxPlan {

// C#: "down" in WSPRtimer_Action - seconds to the start of the next even minute (1..120)
int secondsToNextEven(const QTime &utc);

// Start of the next even minute after utc (an even minute at :00 counts as "now")
QDateTime nextCycleStart(const QDateTime &utc);

// Start of the even minute containing utc (the cycle running now)
QDateTime currentCycleStart(const QDateTime &utc);

// C#: Test TX buttons refuse the receive-only bands (22 m 13.5539 MHz, 8 m 40.xx MHz)
bool isReceiveOnly(double mhz);

// The slot table key for the cycle starting at cycleUtc: local time when the schedule is kept in LT
QDateTime slotKey(const QDateTime &cycleUtc, bool localTime);

// What the once-a-second tick should do at this UTC time (each fires once per cycle; the caller keeps the flags)
bool isLookupWindow(const QTime &utc);    // odd :54-57
bool isPrepareWindow(const QTime &utc);   // odd :55-59 (5 s or less to go)
bool isFireWindow(const QTime &utc);      // even :00-02
bool isBlockMoment(const QTime &utc);     // odd :45

// C#: activateAntSwitch - the antenna by name, else the default antenna, else nothing
std::optional<AntennaInfo> antennaFor(const QVector<AntennaInfo> &ants, const QString &name, const QString &defaultName);

enum class SwitchKind { None, Arduino, W410A, Unsupported };
SwitchKind switchKind(const HardwareInfo &h);

struct Result
{
    bool ok = true;
    QString message;   // what was done, or why it failed
};

// One switch: sw = index into switches (0 = none), port = 0-based port (antenna editor list row).
// Arduino: pin = port (C#: channel - 1); W-410A: "W410A0+P<port+1>".
Result setSwitch(const QVector<HardwareInfo> &switches, int sw, int port, int timeoutMs = 2000);
// Both switches of an antenna (C#: changeAntenna)
Result selectAntenna(const AntennaInfo &ant, const QVector<HardwareInfo> &switches, int timeoutMs = 2000);
// C#: changeFilter - only Arduino filter boards are driven; freqHz as C# ("14095600")
Result selectFilter(const QVector<HardwareInfo> &filters, int selected, qint64 freqHz, int timeoutMs = 2000);

} // namespace TxPlan
