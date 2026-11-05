#include "othertx.h"
#include "w410a.h"   // serial settings parsing

#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSerialPort>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace OtherTx {

QString baseUrl(const RigLink &rig)
{
    QString api = rig.apiPath.trimmed();
    while (api.startsWith('/'))
        api.remove(0, 1);
    return QString("%1://%2:%3/%4").arg(rig.https ? "https" : "http", rig.ip.trimmed(), rig.port.trimmed(), api);
}

static Result sendHttp(const RigLink &rig, const QString &commands, int timeoutMs)
{
    Result r;
    const QString base = baseUrl(rig);
    QNetworkAccessManager nam;
    QStringList replies;
    for (QString cmd : commands.split(';')) {
        cmd = cmd.trimmed();
        if (cmd.isEmpty())
            continue;   // trailing ';' or blank part: C# sent a request with no command
        const QUrl url(base + QString(cmd).replace(' ', '+'));
        if (!url.isValid()) {
            r.error = "Invalid URL " + url.toString();
            return r;
        }
        QNetworkRequest req(url);
        req.setTransferTimeout(timeoutMs);
        QNetworkReply *rep = nam.get(req);
        QEventLoop loop;
        QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(timeoutMs + 500, &loop, &QEventLoop::quit);   // belt and braces
        if (!rep->isFinished())
            loop.exec();
        const int status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (!rep->isFinished() || rep->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            const bool timedOut = !rep->isFinished() || rep->error() == QNetworkReply::OperationCanceledError
                                  || rep->error() == QNetworkReply::TimeoutError;
            r.error = QString("Rig command \"%1\" failed: %2")
                          .arg(cmd, timedOut ? QString("no answer within %1 ms").arg(timeoutMs)
                                    : status ? QString("HTTP %1").arg(status)
                                             : rep->errorString());
            rep->abort();
            rep->deleteLater();
            r.reply = replies.join('\n');
            return r;
        }
        replies << QString::fromUtf8(rep->readAll()).trimmed();
        rep->deleteLater();
    }
    if (replies.isEmpty()) {
        r.error = "No command to send";
        return r;
    }
    r.ok = true;
    r.reply = replies.join('\n');
    r.replies = replies;
    return r;
}

static Result sendSerial(const RigLink &rig, const QString &commands, int timeoutMs)
{
    Result r;
    if (commands.trimmed().isEmpty()) {
        r.error = "No command to send";
        return r;
    }
    QString err;
    const auto s = W410A::parseSettings(rig.port, rig.baud, rig.serial, &err);
    if (!s) {
        r.error = "Rig serial settings: " + err;
        return r;
    }
    QSerialPort p;
    p.setPortName(s->port);
    p.setBaudRate(s->baud);
    p.setDataBits(QSerialPort::DataBits(s->dataBits));
    p.setParity(s->parity == "even" ? QSerialPort::EvenParity
                : s->parity == "odd" ? QSerialPort::OddParity
                                     : QSerialPort::NoParity);
    p.setStopBits(s->stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
    const QString flow = s->flow.toLower();
    p.setFlowControl(flow == "xon/xoff" ? QSerialPort::SoftwareControl
                     : flow == "rts"    ? QSerialPort::HardwareControl
                                        : QSerialPort::NoFlowControl);
    if (!p.open(QIODevice::ReadWrite)) {
        r.error = QString("Cannot open %1 for the rig: %2").arg(s->port, p.errorString());
        return r;
    }
    const QByteArray out = commands.trimmed().toLatin1() + '\n';   // C# WriteLine
    p.write(out);
    if (!p.waitForBytesWritten(timeoutMs)) {
        r.error = QString("Could not write to %1: %2").arg(s->port, p.errorString());
        return r;
    }
    // optional reply: collect whatever arrives within 500 ms (C# waited 500 ms then read)
    QByteArray in;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 500 && p.waitForReadyRead(int(500 - t.elapsed())))
        in += p.readAll();
    r.reply = QString::fromLatin1(in).trimmed();
    r.replies = QStringList{r.reply};
    r.ok = true;
    return r;
}

Result send(const RigLink &rig, const QString &commands, int timeoutMs)
{
    if (rig.protocol.contains("Serial"))
        return sendSerial(rig, commands, timeoutMs);
    return sendHttp(rig, commands, timeoutMs);
}

} // namespace OtherTx
