// TxPlan: cycle timing, antenna fallback and switch/filter selection against a fake Arduino board
// (in its own thread, as the switch calls block). No hardware or database needed.
#include "txplan.h"
#include "rigctld.h"

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimeZone>
#include <QMutex>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Answers like the Arduino sketch: "set-pin n high" -> "Pin: n set: high", "filter:f" -> "ok"
class FakeBoard : public QObject
{
public:
    QTcpServer *server = nullptr;
    QMutex lock;
    QStringList commands;
    void start()
    {
        server = new QTcpServer(this);
        server->listen(QHostAddress::LocalHost);
        connect(server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *c = server->nextPendingConnection();
            connect(c, &QTcpSocket::readyRead, c, [this, c] {
                if (!c->canReadLine()) return;
                const QString cmd = QString::fromLatin1(c->readLine()).trimmed();
                { QMutexLocker l(&lock); commands << cmd; }
                QString reply = "error";
                const QStringList p = cmd.split(' ');
                if (p.size() == 3 && p[0] == "set-pin") reply = QString("Pin: %1 set: %2").arg(p[1], p[2]);
                else if (cmd.startsWith("filter:")) reply = "ok";
                c->write((reply + "\r\n").toLatin1());
                c->disconnectFromHost();
            });
        });
    }
    QStringList take() { QMutexLocker l(&lock); QStringList c = commands; commands.clear(); return c; }
};

static QDateTime utc(int h, int m, int s) { return QDateTime(QDate(2026, 11, 2), QTime(h, m, s), QTimeZone::utc()); }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace TxPlan;

    // ---- timing (C#: WSPRtimer "down") ----
    CHECK(secondsToNextEven(QTime(10, 1, 55)) == 5);
    CHECK(secondsToNextEven(QTime(10, 1, 0)) == 60);
    CHECK(secondsToNextEven(QTime(10, 2, 0)) == 120);
    CHECK(secondsToNextEven(QTime(10, 2, 1)) == 119);
    CHECK(secondsToNextEven(QTime(23, 59, 59)) == 1);
    CHECK(nextCycleStart(utc(10, 1, 54)) == utc(10, 2, 0));
    CHECK(nextCycleStart(utc(10, 2, 0)) == utc(10, 2, 0));
    CHECK(nextCycleStart(utc(10, 2, 30)) == utc(10, 4, 0));
    CHECK(nextCycleStart(utc(23, 59, 56)) == QDateTime(QDate(2026, 11, 3), QTime(0, 0), QTimeZone::utc()));
    CHECK(slotKey(utc(10, 2, 0), false) == utc(10, 2, 0) && slotKey(utc(10, 2, 0), false).timeSpec() == Qt::UTC);
    CHECK(isLookupWindow(QTime(10, 1, 54)) && isLookupWindow(QTime(10, 1, 57)) && !isLookupWindow(QTime(10, 1, 58))
          && !isLookupWindow(QTime(10, 2, 54)));
    CHECK(isPrepareWindow(QTime(10, 1, 55)) && isPrepareWindow(QTime(10, 1, 59)) && !isPrepareWindow(QTime(10, 1, 54)));
    CHECK(isFireWindow(QTime(10, 2, 0)) && isFireWindow(QTime(10, 2, 2)) && !isFireWindow(QTime(10, 2, 3))
          && !isFireWindow(QTime(10, 1, 0)));
    CHECK(isBlockMoment(QTime(10, 3, 45)) && !isBlockMoment(QTime(10, 2, 45)));
    CHECK(currentCycleStart(utc(10, 3, 59)) == utc(10, 2, 0) && currentCycleStart(utc(10, 2, 0)) == utc(10, 2, 0));
    CHECK(isReceiveOnly(13.5539) && isReceiveOnly(40.68) && !isReceiveOnly(14.0956) && !isReceiveOnly(50.293));

    // ---- rig reference correction: G4GCI's FT-450D reads 0.85 ppm low ----
    CHECK(correctedDialHz(14095600, 0.85) == 14095612 && correctedDialHz(50293000, 0.85) == 50293043);
    CHECK(correctedDialHz(14095600, 0.0) == 14095600 && correctedDialHz(14095600, -1.0) == 14095586);
    CHECK(actualFromDialHz(14095612, 0.85) == 14095600 && actualFromDialHz(50293043, 0.85) == 50293000);

    // ---- rigctld on this PC or another
    CHECK(Hamlib::isLocalAddress("127.0.0.1") && Hamlib::isLocalAddress("localhost") && Hamlib::isLocalAddress(""));
    CHECK(Hamlib::isLocalAddress("::1") && !Hamlib::isLocalAddress("192.0.2.10") && !Hamlib::isLocalAddress("shackpc"));

    // ---- antenna by name, else the default (C#: activateAntSwitch) ----
    QVector<AntennaInfo> ants(2);
    ants[0].name = "Windom"; ants[0].sw = 1; ants[0].swPort = 2;
    ants[1].name = "Vertical"; ants[1].sw = 1; ants[1].swPort = 0; ants[1].sw2 = 2; ants[1].swPort2 = 3;
    CHECK(antennaFor(ants, "Vertical", "Windom")->name == "Vertical");
    CHECK(antennaFor(ants, "Gone", "Windom")->name == "Windom");
    CHECK(!antennaFor(ants, "Gone", "Also gone"));

    // ---- switches ----
    QThread boardThread;
    FakeBoard board;
    board.moveToThread(&boardThread);
    boardThread.start();
    QMetaObject::invokeMethod(&board, [&] { board.start(); }, Qt::BlockingQueuedConnection);
    const QString port = QString::number(board.server->serverPort());

    QVector<HardwareInfo> sw(4);
    sw[0].name = "No switch";
    sw[1].id = 1; sw[1].name = "Arduino shack"; sw[1].type = "Arduino R4 wifi (antenna switchable on data pins)";
    sw[1].protocol = "TCP"; sw[1].ip = "127.0.0.1"; sw[1].port = port;
    sw[2].id = 2; sw[2].name = "Arduino 2"; sw[2].type = sw[1].type; sw[2].ip = "127.0.0.1"; sw[2].port = port;
    sw[3].id = 3; sw[3].name = "Manual box"; sw[3].type = "Manual";
    CHECK(switchKind(sw[0]) == SwitchKind::None && switchKind(sw[1]) == SwitchKind::Arduino
          && switchKind(sw[3]) == SwitchKind::Unsupported);
    HardwareInfo w; w.id = 1; w.type = "W-410A (antenna selectable by commands)";
    CHECK(switchKind(w) == SwitchKind::W410A);

    auto r = selectAntenna(ants[0], sw);
    CHECK(r.ok && board.take() == QStringList{"set-pin 2 high"});    // port 3 in the editor -> pin 2 (C#: channel - 1)
    std::printf("     %s\n", qPrintable(r.message));
    r = selectAntenna(ants[1], sw);
    CHECK(r.ok && board.take() == (QStringList{"set-pin 0 high", "set-pin 3 high"}));   // both switches
    std::printf("     %s\n", qPrintable(r.message));

    AntennaInfo none; none.name = "Loop";
    r = selectAntenna(none, sw);
    CHECK(r.ok && board.take().isEmpty() && r.message.contains("no switch"));

    AntennaInfo manual; manual.name = "Manual"; manual.sw = 3;
    r = selectAntenna(manual, sw);
    CHECK(!r.ok && r.message.contains("can't be switched"));

    AntennaInfo missing; missing.name = "X"; missing.sw = 7;
    CHECK(!selectAntenna(missing, sw).ok);

    sw[1].port = "COM3";                              // C#: "Port error - should be a number"
    r = selectAntenna(ants[0], sw);
    CHECK(!r.ok && r.message.contains("should be a number"));
    sw[1].port = "1";                                 // nothing listening: TX must not go ahead
    r = selectAntenna(ants[0], sw, 500);
    CHECK(!r.ok);
    std::printf("     dead switch: %s\n", qPrintable(r.message));

    QVector<HardwareInfo> w4(2);
    w4[1] = w; w4[1].port = "COM_DOES_NOT_EXIST"; w4[1].baud = "9600"; w4[1].serial = "8,none,1,none";
    AntennaInfo loft; loft.name = "Loft"; loft.sw = 1; loft.swPort = 1;
    r = selectAntenna(loft, w4, 300);
    CHECK(!r.ok);
    std::printf("     W-410A missing: %s\n", qPrintable(r.message));

    // ---- filter ----
    QVector<HardwareInfo> fil(2);
    fil[1].id = 1; fil[1].name = "LPF board"; fil[1].type = "Arduino - single action on data pin 1";
    fil[1].ip = "127.0.0.1"; fil[1].port = port;
    r = selectFilter(fil, 1, 14095600);
    CHECK(r.ok && board.take() == QStringList{"filter:14095600"});
    CHECK(selectFilter(fil, 0, 14095600).ok && board.take().isEmpty());      // none selected
    CHECK(selectFilter(fil, 5, 14095600).ok);                                 // out of range: ignored, as C#

    boardThread.quit();
    boardThread.wait();
    std::printf(failures ? "txplan_test: %d FAILED\n" : "txplan_test: all passed\n", failures);
    return failures ? 1 : 0;
}
