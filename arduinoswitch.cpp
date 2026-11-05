#include "arduinoswitch.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTcpSocket>

namespace ArduinoSwitch {

Result exchange(const QString &ip, int port, const QString &command, int timeoutMs)
{
    Result r;
    if (ip.trimmed().isEmpty() || port <= 0 || port > 65535) {
        r.error = "Invalid IP or port";
        return r;
    }
    QTcpSocket s;
    s.connectToHost(ip.trimmed(), quint16(port));
    if (!s.waitForConnected(timeoutMs)) {
        r.error = QString("Cannot connect to switch at %1:%2 (%3)").arg(ip).arg(port).arg(s.errorString());
        return r;
    }
    const QByteArray out = command.toLatin1() + '\n';
    s.write(out);
    if (!s.waitForBytesWritten(timeoutMs)) {
        r.error = "Could not send to switch: " + s.errorString();
        return r;
    }
    QByteArray in;
    QElapsedTimer t;
    t.start();
    while (!in.contains('\n')) {
        const int left = timeoutMs - int(t.elapsed());
        if (left <= 0)
            break;
        if (!s.waitForReadyRead(left)) {
            in += s.readAll();
            break;   // timeout, or the board closed the connection
        }
        in += s.readAll();
    }
    r.reply = QString::fromLatin1(in).section('\n', 0, 0).trimmed();
    if (r.reply.isEmpty()) {
        r.error = QString("No reply from switch at %1:%2 to \"%3\"").arg(ip).arg(port).arg(command);
        return r;
    }
    r.ok = true;
    return r;
}

Result setPin(const QString &ip, int port, int pin, bool high, int timeoutMs)
{
    if (pin < 0 || pin > 13) {
        Result r;
        r.error = QString("Pin %1 out of range 0-13").arg(pin);
        return r;
    }
    const QString level = high ? "high" : "low";
    Result r = exchange(ip, port, QString("set-pin %1 %2").arg(pin).arg(level), timeoutMs);
    if (!r.ok)
        return r;
    // The sketch answers "Pin: <n> set: <level>"; anything else (e.g. "error") is a failure.
    static const QRegularExpression re(QStringLiteral("^Pin:\\s*(\\d+)\\s+set:\\s*(high|low)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(r.reply);
    if (!m.hasMatch()) {
        r.ok = false;
        r.error = QString("Switch did not confirm set-pin %1 %2 (reply \"%3\")").arg(pin).arg(level, r.reply);
    } else if (m.captured(1).toInt() != pin || m.captured(2).toLower() != level) {
        r.ok = false;
        r.error = QString("Switch set pin %1 %2 instead of pin %3 %4 - the sketch only reads one digit; "
                          "update it for pins 10-13")
                      .arg(m.captured(1), m.captured(2)).arg(pin).arg(level);
    }
    return r;
}

Result setFilter(const QString &ip, int port, const QString &freq, int timeoutMs)
{
    Result r = exchange(ip, port, "filter:" + freq.trimmed(), timeoutMs);
    if (r.ok && r.reply.compare("error", Qt::CaseInsensitive) == 0) {
        r.ok = false;
        r.error = QString("Filter board did not accept \"filter:%1\"").arg(freq.trimmed());
    }
    return r;
}

} // namespace ArduinoSwitch
