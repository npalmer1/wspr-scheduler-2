// RigCtl against Hamlib's real rigctld using its dummy rig (-m 1). Skips if rigctld isn't installed.
#include "rigctl.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static QProcess *startRigctld(int port, bool pttCapable = true)
{
    auto *p = new QProcess;
    QStringList args{"-m", "1", "-t", QString::number(port)};
    if (pttCapable)
        args << "-P" << "RIG";   // without this the dummy rig refuses PTT with RPRT -1
    p->start(QStandardPaths::findExecutable("rigctld"), args);
    p->waitForStarted();
    for (int i = 0; i < 50; ++i) {   // wait until it accepts connections
        QTcpSocket s;
        s.connectToHost("127.0.0.1", quint16(port));
        if (s.waitForConnected(100)) return p;
        QThread::msleep(100);
    }
    return p;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (QStandardPaths::findExecutable("rigctld").isEmpty()) {
        std::printf("rigctld not installed - skipped\n");
        return 77;
    }
    const int port = 45320;
    QProcess *d = startRigctld(port);
    RigCtl rig("127.0.0.1", port);

    CHECK(rig.isReachable());
    CHECK(rig.setFrequency(14095600));
    CHECK(rig.frequency() == 14095600);
    CHECK(rig.setFrequency(7038600));
    CHECK(rig.frequency() == 7038600);                 // same connection reused
    CHECK(rig.setPtt(true));
    CHECK(rig.ptt() == true);
    CHECK(rig.setPtt(false));
    CHECK(rig.ptt() == false);

    // errors that C# reported as "ok"
    const auto bad = rig.send("F notanumber");
    CHECK(bad && RigCtl::rprtCode(*bad).value_or(0) < 0);
    std::printf("     rigctld answer to a bad frequency: \"%s\"\n", qPrintable(bad.value_or("?")));
    CHECK(!rig.setFrequency(0));
    CHECK(RigCtl::rprtCode("RPRT 0") == 0 && RigCtl::rprtCode("RPRT -11") == -11 && !RigCtl::rprtCode("14095600"));

    // a rig that refuses PTT: C# sendTXRigCommand returned "ok" here and played audio unkeyed
    {
        QProcess *noPtt = startRigctld(port + 1, false);
        RigCtl r2("127.0.0.1", port + 1);
        CHECK(r2.setFrequency(14095600));
        CHECK(!r2.setPtt(true));
        std::printf("     PTT refused: %s\n", qPrintable(r2.lastError()));
        noPtt->kill(); noPtt->waitForFinished(); delete noPtt;
    }

    // rigctld restarted between commands -> reconnects by itself
    d->kill(); d->waitForFinished(); delete d;
    CHECK(!rig.setFrequency(10138700));                // down: refused, with a message
    std::printf("     while stopped: %s\n", qPrintable(rig.lastError()));
    d = startRigctld(port);
    CHECK(rig.setFrequency(10138700) && rig.frequency() == 10138700);

    // a server that accepts but never answers -> timeout instead of hanging
    QTcpServer silent;
    silent.listen(QHostAddress::LocalHost);
    QObject::connect(&silent, &QTcpServer::newConnection, [&] { silent.nextPendingConnection(); });
    RigCtl hang("127.0.0.1", silent.serverPort());
    hang.setTimeout(500);
    QElapsedTimer t; t.start();
    bool gotReply = false;
    // the silent server needs the event loop to accept; run the request from a worker thread
    QThread *w = QThread::create([&] { gotReply = hang.setFrequency(14095600); });
    w->start();
    while (!w->isFinished()) { app.processEvents(); QThread::msleep(5); }
    delete w;
    CHECK(!gotReply && t.elapsed() < 3000);
    std::printf("     silent server: %s after %lld ms\n", qPrintable(hang.lastError()), t.elapsed());

    RigCtl none("127.0.0.1", 1);                        // nothing listening
    CHECK(!none.isReachable() && none.lastError().contains("Cannot connect"));

    d->kill(); d->waitForFinished(); delete d;
    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
