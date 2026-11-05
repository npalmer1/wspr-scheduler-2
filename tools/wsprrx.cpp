// Receiver test: records WSPR cycles from a sound card exactly as the receive window does and prints
// what wsprd decodes. No database, no upload. Pair it with wsprplay through a virtual cable, or
// listen to the rig's receive audio.
//
//   wsprrx --list
//   wsprrx --device "Microphone (USB Audio Device)" --wsprd "C:/WSJT/wsjtx/bin" --dial 14.0956
//   wsprrx --file C:/WSPR_Sked/temp1.wav --wsprd ...      (decode an existing recording only)
//   options: --cycles N  --gain 1.0  --deep  --quick  --osd N  --data DIR  --call G4GCI --loc IO90GU
#include "rxcycle.h"
#include "wspraudioin.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);
    QCommandLineParser p;
    p.setApplicationDescription("Record WSPR cycles from a sound card and decode them with wsprd");
    p.addHelpOption();
    p.addOptions({{"list", "List sound inputs"},
                  {"device", "Input device (name as shown by --list)", "name"},
                  {"wsprd", "Folder containing wsprd(.exe), or the file itself; default: a wsprd folder next to this program, then PATH", "dir"},
                  {"dial", "Dial frequency in MHz (passed to wsprd -f)", "MHz", "14.0956"},
                  {"data", "Folder for the recordings and wsprd files", "dir", Rx::defaultDataDir()},
                  {"cycles", "Number of 2-minute cycles", "n", "1"},
                  {"gain", "Input gain (C#: RX & Sound level / 100)", "g", "1.0"},
                  {"deep", "wsprd -d"},
                  {"quick", "wsprd -q"},
                  {"osd", "wsprd -o N (1-5)", "n", "0"},
                  {"call", "Your call (for distance only)", "call", "G4GCI"},
                  {"loc", "Your locator (for distance/azimuth)", "loc", "IO90"},
                  {"file", "Decode this WAV instead of recording", "wav"}});
    p.process(app);

    if (p.isSet("list")) {
        for (const QString &n : WsprAudioIn::inputDeviceNames())
            std::printf("%s\n", qPrintable(n));
        return 0;
    }

    RxCycle rx;
    rx.setDataDir(p.value("data"));
    rx.setWsprdDir(p.value("wsprd"));
    rx.setDial(p.value("dial"));
    rx.setGain(p.value("gain").toFloat());
    rx.setReporter(p.value("call"), p.value("loc"));
    rx.setOptions({p.isSet("deep"), p.isSet("quick"), p.value("osd").toInt()});
    if (Rx::wsprdExecutable(p.value("wsprd")).isEmpty()) {
        std::fprintf(stderr, "wsprd not found - give its folder with --wsprd\n");
        return 1;
    }
    std::printf("wsprd: %s\n", qPrintable(Rx::wsprdExecutable(p.value("wsprd"))));

    int remaining = qMax(1, p.value("cycles").toInt());
    int exitCode = 0;
    QObject::connect(&rx, &RxCycle::statusChanged, [](const QString &s) {
        std::printf("%s  %s\n", qPrintable(QDateTime::currentDateTimeUtc().toString("HH:mm:ss")), qPrintable(s));
    });
    QObject::connect(&rx, &RxCycle::problem, [&](const QString &m) {
        std::fprintf(stderr, "%s\n", qPrintable(m));
        exitCode = 2;
    });
    QObject::connect(&rx, &RxCycle::decoded, [&](const Rx::DecodeResult &r, const QString &dial, const QDateTime &cyc) {
        std::printf("-- %s UTC, dial %s MHz: %d line(s)\n", qPrintable(cyc.toString("yyyy-MM-dd HH:mm")),
                    qPrintable(dial), int(r.spots.size()));
        for (const RxSpot &s : r.spots) {
            if (s.isNil()) {
                std::printf("   nil rcvd\n");
                continue;
            }
            std::printf("   %4s %5.1f %11.6f %3d  %-12s %-6s %3d  %s km %s deg\n", qPrintable(Rx::snrText(s.snr)), s.dt,
                        s.frequency, s.drift, qPrintable(s.txSign), qPrintable(s.txLoc), s.power,
                        s.distance >= 0 ? qPrintable(QString::number(s.distance)) : "-",
                        s.azimuth >= 0 ? qPrintable(QString::number(s.azimuth)) : "-");
        }
    });

    if (p.isSet("file")) {
        QObject::connect(&rx, &RxCycle::statusChanged, &app, [&](const QString &s) {
            if (s != "decoding")
                app.exit(exitCode);
        });
        if (!rx.decodeFile(p.value("file"), Rx::cycleStart(QDateTime::currentDateTimeUtc()), p.value("dial"))) {
            std::fprintf(stderr, "cannot decode: %s\n", qPrintable(rx.lastError()));
            return 1;
        }
        return app.exec();
    }

    if (!rx.setDevice(p.value("device"))) {
        std::fprintf(stderr, "No input called \"%s\" - try --list\n", qPrintable(p.value("device")));
        return 1;
    }
    std::printf("input: %s\n", qPrintable(rx.deviceName()));

    // Start at odd:59 / even:00 like the main window does, then decode while the next cycle records
    QTimer tick;
    bool started = false;
    QObject::connect(&tick, &QTimer::timeout, [&] {
        const QTime t = QDateTime::currentDateTimeUtc().time();
        const bool trigger = (t.minute() % 2 == 1 && t.second() == 59) || (t.minute() % 2 == 0 && t.second() <= 1);
        if (trigger && !started && remaining > 0 && !rx.isRecording()) {
            started = true;
            if (rx.start())
                --remaining;
            else
                std::fprintf(stderr, "cannot record: %s\n", qPrintable(rx.lastError()));
        }
        if (t.second() >= 7 && t.second() <= 9)
            started = false;
        if (remaining == 0 && !rx.isRecording() && !rx.isDecoding())
            app.exit(exitCode);
    });
    tick.start(250);
    std::printf("waiting for the next even minute (UTC)...\n");
    return app.exec();
}
