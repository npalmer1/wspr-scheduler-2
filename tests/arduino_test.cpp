// ArduinoSwitch against a fake board that answers like the sketch in Arduino.cs (including its
// one-digit pin bug), plus a silent board and nothing listening. No hardware needed.
#include "arduinoswitch.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Replies exactly as the original sketch: one digit of the pin, "error" for anything else
static QString sketchReply(const QString &line, bool fixedSketch)
{
    if (!line.startsWith("set-pin "))
        return "error";
    const QString rest = line.mid(8);
    const QString d = fixedSketch ? rest.section(' ', 0, 0) : rest.left(1);
    return QString("Pin: %1 set: %2").arg(d, rest.indexOf("high") > 0 ? "high" : "low");
}

template <typename F> static ArduinoSwitch::Result inWorker(QCoreApplication &app, F f)
{
    ArduinoSwitch::Result r;
    QThread *w = QThread::create([&] { r = f(); });
    w->start();
    while (!w->isFinished()) { app.processEvents(); QThread::msleep(2); }
    delete w;
    return r;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool fixedSketch = false, silent = false;
    QString lastCommand;
    QTcpServer board;
    board.listen(QHostAddress::LocalHost);
    QObject::connect(&board, &QTcpServer::newConnection, [&] {
        QTcpSocket *c = board.nextPendingConnection();
        QObject::connect(c, &QTcpSocket::readyRead, c, [&, c] {
            if (!c->canReadLine()) return;
            lastCommand = QString::fromLatin1(c->readLine()).trimmed();
            if (silent) return;
            c->write((sketchReply(lastCommand, fixedSketch) + "\r\n").toLatin1());   // client.println -> CRLF
            c->disconnectFromHost();                                                  // client.stop()
        });
    });
    const int port = board.serverPort();
    using namespace ArduinoSwitch;

    auto r = inWorker(app, [&] { return setPin("127.0.0.1", port, 3, true); });
    CHECK(r.ok && r.reply == "Pin: 3 set: high" && lastCommand == "set-pin 3 high");
    r = inWorker(app, [&] { return setPin("127.0.0.1", port, 0, false); });
    CHECK(r.ok && lastCommand == "set-pin 0 low");

    // pin 12 on the original sketch sets pin 1 - must be caught
    r = inWorker(app, [&] { return setPin("127.0.0.1", port, 12, true); });
    CHECK(!r.ok && r.reply == "Pin: 1 set: high");
    std::printf("     original sketch, pin 12: %s\n", qPrintable(r.error));
    fixedSketch = true;
    r = inWorker(app, [&] { return setPin("127.0.0.1", port, 12, true); });
    CHECK(r.ok && r.reply == "Pin: 12 set: high");

    // filter command: this sketch doesn't know it and says "error" (C# reported ok)
    r = inWorker(app, [&] { return setFilter("127.0.0.1", port, "14.0956"); });
    CHECK(!r.ok && lastCommand == "filter:14.0956");
    std::printf("     filter on switch sketch: %s\n", qPrintable(r.error));

    CHECK(!setPin("127.0.0.1", port, 14, true).ok);      // out of range, nothing sent
    CHECK(!setPin("", port, 1, true).ok);

    // silent board -> timeout, not a hang
    silent = true;
    QElapsedTimer t; t.start();
    r = inWorker(app, [&] { return setPin("127.0.0.1", port, 2, true, 500); });
    CHECK(!r.ok && t.elapsed() < 2000);
    std::printf("     silent board: %s (%lld ms)\n", qPrintable(r.error), t.elapsed());

    r = setPin("127.0.0.1", 1, 2, true, 500);             // nothing listening
    CHECK(!r.ok && r.error.contains("Cannot connect"));

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
