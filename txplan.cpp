#include "txplan.h"

#include "arduinoswitch.h"
#include "w410a.h"

#include <QTimeZone>

namespace TxPlan {

int secondsToNextEven(const QTime &utc)
{
    const int m = utc.minute(), s = utc.second();
    if (m % 2 == 0)
        return s == 0 ? 120 : 120 - s;
    return 60 - s;
}

QDateTime nextCycleStart(const QDateTime &utc)
{
    const QDateTime u = utc.toUTC();
    QDateTime t(u.date(), QTime(u.time().hour(), u.time().minute()), QTimeZone::utc());
    if (t.time().minute() % 2 == 1)
        t = t.addSecs(60);
    else if (u.time().second() != 0 || u.time().msec() != 0)
        t = t.addSecs(120);
    return t;
}

QDateTime currentCycleStart(const QDateTime &utc)
{
    const QDateTime u = utc.toUTC();
    QDateTime t(u.date(), QTime(u.time().hour(), u.time().minute()), QTimeZone::utc());
    return t.time().minute() % 2 == 1 ? t.addSecs(-60) : t;
}

bool isReceiveOnly(double mhz)
{
    return (mhz >= 13.0 && mhz < 14.0) || (mhz >= 40.0 && mhz < 41.0);
}

QDateTime slotKey(const QDateTime &cycleUtc, bool localTime)
{
    return localTime ? cycleUtc.toLocalTime() : cycleUtc.toUTC();
}

bool isLookupWindow(const QTime &t) { return t.minute() % 2 == 1 && t.second() >= 54 && t.second() <= 57; }
bool isPrepareWindow(const QTime &t) { return t.minute() % 2 == 1 && t.second() >= 55; }
bool isFireWindow(const QTime &t) { return t.minute() % 2 == 0 && t.second() <= 2; }
bool isBlockMoment(const QTime &t) { return t.minute() % 2 == 1 && t.second() == 45; }

std::optional<AntennaInfo> antennaFor(const QVector<AntennaInfo> &ants, const QString &name, const QString &defaultName)
{
    for (const AntennaInfo &a : ants)
        if (a.name == name)
            return a;
    for (const AntennaInfo &a : ants)
        if (a.name == defaultName)
            return a;
    return std::nullopt;
}

SwitchKind switchKind(const HardwareInfo &h)
{
    if (h.type.trimmed().isEmpty() || h.id == 0)
        return SwitchKind::None;
    if (h.type.startsWith("Arduino", Qt::CaseInsensitive))
        return SwitchKind::Arduino;
    if (h.type.startsWith("W-410A", Qt::CaseInsensitive))
        return SwitchKind::W410A;
    return SwitchKind::Unsupported;
}

Result setSwitch(const QVector<HardwareInfo> &switches, int sw, int port, int timeoutMs)
{
    if (sw <= 0)
        return {true, QString()};
    if (sw >= switches.size())
        return {false, QString("Switch %1 is not set up (Switches&Tuners tab)").arg(sw)};
    const HardwareInfo &h = switches[sw];
    switch (switchKind(h)) {
    case SwitchKind::None:
        return {true, QString()};
    case SwitchKind::Arduino: {
        bool okPort = false;
        const int tcp = h.port.trimmed().toInt(&okPort);
        if (!okPort || h.port.startsWith("COM", Qt::CaseInsensitive))   // C#: "Port error - should be a number"
            return {false, QString("Switch %1 (%2): port should be a number, not \"%3\"").arg(sw).arg(h.name, h.port)};
        const auto r = ArduinoSwitch::setPin(h.ip.trimmed(), tcp, port, true, timeoutMs);
        if (!r.ok)
            return {false, QString("Switch %1 (%2): %3").arg(sw).arg(h.name, r.error)};
        return {true, QString("Switch %1 port %2").arg(sw).arg(port + 1)};
    }
    case SwitchKind::W410A: {
        QString err;
        const auto s = W410A::parseSettings(h.port.trimmed(), h.baud, h.serial, &err);
        if (!s)
            return {false, QString("Switch %1 (%2): %3").arg(sw).arg(h.name, err)};
        const auto r = W410A::send(*s, QString("W410A0+P%1").arg(port + 1), timeoutMs);
        if (!r.ok)
            return {false, QString("Switch %1 (%2): %3").arg(sw).arg(h.name, r.error)};
        return {true, QString("W-410A port %1").arg(port + 1)};
    }
    case SwitchKind::Unsupported:
        break;
    }
    return {false, QString("Switch %1 (%2): type \"%3\" can't be switched by the program").arg(sw).arg(h.name, h.type)};
}

Result selectAntenna(const AntennaInfo &ant, const QVector<HardwareInfo> &switches, int timeoutMs)
{
    const Result a = setSwitch(switches, ant.sw, ant.swPort, timeoutMs);
    if (!a.ok)
        return a;
    const Result b = setSwitch(switches, ant.sw2, ant.swPort2, timeoutMs);
    if (!b.ok)
        return b;
    QStringList parts;
    for (const QString &m : {a.message, b.message})
        if (!m.isEmpty())
            parts << m;
    return {true, ant.name + (parts.isEmpty() ? QString(" (no switch)") : ": " + parts.join(" + "))};
}

Result selectFilter(const QVector<HardwareInfo> &filters, int selected, qint64 freqHz, int timeoutMs)
{
    if (selected <= 0 || selected >= filters.size())
        return {true, QString()};
    const HardwareInfo &h = filters[selected];
    if (!h.type.startsWith("Arduino", Qt::CaseInsensitive))
        return {true, QString()};   // as C#: only Arduino filter boards are driven
    bool okPort = false;
    const int tcp = h.port.trimmed().toInt(&okPort);
    if (!okPort)
        return {false, QString("Filter %1: port should be a number, not \"%2\"").arg(h.name, h.port)};
    const auto r = ArduinoSwitch::setFilter(h.ip.trimmed(), tcp, QString::number(freqHz), timeoutMs);
    if (!r.ok)
        return {false, QString("Filter %1: %2").arg(h.name, r.error)};
    return {true, "Filter " + h.name};
}

} // namespace TxPlan
