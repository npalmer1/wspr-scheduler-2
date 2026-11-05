#include "w410a.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSerialPort>
#include <QStringList>
#include <QThread>

namespace W410A {

std::optional<SerialSettings> parseSettings(const QString &port, const QString &baud, const QString &spec,
                                            QString *error)
{
    auto fail = [&](const QString &e) {
        if (error) *error = e;
        return std::optional<SerialSettings>();
    };
    SerialSettings s;
    s.port = port.trimmed();
    if (s.port.isEmpty())
        return fail("No COM port set for the switch");
    bool ok = false;
    s.baud = baud.trimmed().toInt(&ok);
    if (!ok || s.baud <= 0)
        return fail("Invalid baud rate \"" + baud + "\"");
    const QStringList f = spec.split(',');
    if (f.size() < 4)
        return fail("Serial settings should be data,parity,stop,flow e.g. 8,none,1,none (got \"" + spec + "\")");
    s.dataBits = f[0].trimmed().toInt(&ok);
    if (!ok || s.dataBits < 5 || s.dataBits > 8)
        return fail("Invalid data bits \"" + f[0] + "\"");
    s.parity = f[1].trimmed().toLower();
    if (s.parity != "none" && s.parity != "even" && s.parity != "odd")
        return fail("Invalid parity \"" + f[1] + "\"");
    s.stopBits = f[2].trimmed().toInt(&ok);
    if (!ok || (s.stopBits != 1 && s.stopBits != 2))
        return fail("Invalid stop bits \"" + f[2] + "\"");
    s.flow = f[3].trimmed();
    return s;
}

Result send(const SerialSettings &s, const QString &message, int timeoutMs)
{
    Result r;
    QSerialPort p;
    p.setPortName(s.port);
    p.setBaudRate(s.baud);
    p.setDataBits(QSerialPort::DataBits(s.dataBits));
    p.setParity(s.parity == "even" ? QSerialPort::EvenParity
                : s.parity == "odd" ? QSerialPort::OddParity
                                    : QSerialPort::NoParity);
    p.setStopBits(s.stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
    const QString flow = s.flow.toLower();
    p.setFlowControl(flow == "xon/xoff" ? QSerialPort::SoftwareControl
                     : flow == "rts"    ? QSerialPort::HardwareControl
                                        : QSerialPort::NoFlowControl);
    if (!p.open(QIODevice::ReadWrite)) {
        r.error = QString("Cannot open %1 for the W-410A switch: %2").arg(s.port, p.errorString());
        return r;
    }
    const QByteArray out = message.toLatin1() + '\n';   // C# SerialPort.WriteLine -> LF
    p.write(out);
    if (!p.waitForBytesWritten(timeoutMs)) {
        r.error = QString("Could not write to %1: %2").arg(s.port, p.errorString());
        return r;
    }
    QByteArray in;
    QElapsedTimer t;
    t.start();
    while (!in.contains('\n') && !in.contains('\r')) {
        const int left = timeoutMs - int(t.elapsed());
        if (left <= 0 || !p.waitForReadyRead(left))
            break;
        in += p.readAll();
    }
    // allow a CR to be followed by its LF
    if (in.contains('\r') && !in.contains('\n') && p.waitForReadyRead(50))
        in += p.readAll();
    r.reply = QString::fromLatin1(in).section(QRegularExpression("[\r\n]"), 0, 0).trimmed();
    if (r.reply.isEmpty()) {
        r.error = QString("No reply from the W-410A switch on %1 to \"%2\"").arg(s.port, message);
        return r;
    }
    r.ok = true;
    return r;
}

} // namespace W410A
