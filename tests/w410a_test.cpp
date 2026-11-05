// W410A over a virtual serial pair (socat) with a fake switch answering on the other end.
// Linux/macOS only (needs socat and python3); skipped otherwise.
#include "w410a.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace W410A;

    // settings string parsing (as stored by the C# app)
    QString err;
    auto st = parseSettings("COM3", "9600", "8,none,1,none", &err);
    CHECK(st && st->dataBits == 8 && st->parity == "none" && st->stopBits == 1 && st->flow == "none");
    CHECK(parseSettings("COM3", "4800", "7,even,2,RTS") && parseSettings("COM3", "4800", "7,even,2,RTS")->flow == "RTS");
    CHECK(!parseSettings("COM3", "9600", "8,none,1", &err));          // C# would throw IndexOutOfRange
    CHECK(!parseSettings("", "9600", "8,none,1,none"));
    CHECK(!parseSettings("COM3", "fast", "8,none,1,none"));
    CHECK(!parseSettings("COM3", "9600", "8,mark,1,none"));

    // missing port: C# put the exception text in reply and Form1 treated it as switched
    SerialSettings none{"/dev/does-not-exist", 9600, 8, "none", 1, "none"};
    auto r = setAntenna(none, 2);
    CHECK(!r.ok && r.error.contains("Cannot open"));
    std::printf("     missing port: %s\n", qPrintable(r.error));

    if (QStandardPaths::findExecutable("socat").isEmpty() || QStandardPaths::findExecutable("python3").isEmpty()) {
        std::printf("socat/python3 not found - serial loopback part skipped\n");
        return failures ? 1 : 77;
    }
    QTemporaryDir dir;
    const QString a = dir.filePath("ttyA"), b = dir.filePath("ttyB"), log = dir.filePath("log");
    QProcess socat;
    socat.start("socat", {"pty,raw,echo=0,link=" + a, "pty,raw,echo=0,link=" + b});
    for (int i = 0; i < 50 && !(QFile::exists(a) && QFile::exists(b)); ++i) QThread::msleep(50);

    // fake switch: answers "OK P<n>" with CR only (C# ReadLine would have timed out on that)
    const QString fake = QString(R"(
import os, sys
fd = os.open('%1', os.O_RDWR)
buf = b''
n = 0
while n < 3:
    buf += os.read(fd, 100)
    while b'\n' in buf:
        line, buf = buf.split(b'\n', 1)
        open('%2', 'a').write(line.decode() + '\n')
        n += 1
        if n == 3: break          # third command: stay silent
        os.write(fd, b'OK ' + line[7:] + b'\r')
)").arg(b, log);
    QProcess dev;
    dev.start("python3", {"-c", fake});
    dev.waitForStarted();
    QThread::msleep(200);

    SerialSettings s{a, 9600, 8, "none", 1, "none"};
    r = setAntenna(s, 3);
    CHECK(r.ok && r.reply == "OK P3");
    r = setMode(s, 1);
    CHECK(r.ok && r.reply == "OK M1");
    r = send(s, "W410A0+P4", 500);
    CHECK(!r.ok && r.error.contains("No reply"));
    std::printf("     silent switch: %s\n", qPrintable(r.error));

    dev.waitForFinished(2000);
    QFile f(log);
    f.open(QIODevice::ReadOnly);
    CHECK(f.readAll() == "W410A0+P3\nW410A0+M1\nW410A0+P4\n");   // exact commands as the C# app sends
    socat.kill();
    socat.waitForFinished();

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
