// Encoder test: plays a WSPR message on a sound card at the next even minute + 1 s (UTC),
// so WSJT-X can decode it - e.g. through a virtual audio cable on the same PC.
// Audio only: no rig control, no PTT.
//
//   wsprplay --list
//   wsprplay "G4GCI IO90 23"
//   wsprplay "G4GCI/P 23" --device "CABLE Input" --offset 120 --volume 0.3
//   wsprplay "<G4GCI/P> IO90GU 23" --now          (start immediately - WSJT-X won't decode it)
//   options: --cpfsk (continuous-phase shaping)  --test (100 Hz test tones)  --repeat N
#include "wspraudioout.h"
#include "wsprencode.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);
    QCommandLineParser p;
    p.setApplicationDescription("Play a WSPR message on a sound card for decoding in WSJT-X");
    p.addHelpOption();
    p.addPositionalArgument("message", "\"CALL GRID dBm\", \"PFX/CALL dBm\" or \"<CALL> GRID6 dBm\"");
    p.addOptions({{"list", "List sound outputs"},
                  {"device", "Output device (name as shown by --list)", "name"},
                  {"offset", "Audio offset 0-200 Hz (tone 0 = 1400 + offset)", "Hz", "50"},
                  {"volume", "0.0 - 1.0", "v", "0.5"},
                  {"cpfsk", "Continuous-phase shaping instead of the FSK.cs algorithm"},
                  {"test", "100 Hz test tones (audible, not decodable)"},
                  {"now", "Start immediately instead of at the next even minute"},
                  {"repeat", "Number of transmissions", "n", "1"}});
    p.process(app);

    if (p.isSet("list")) {
        for (const QString &n : WsprAudioOut::outputDeviceNames())
            std::printf("%s\n", qPrintable(n));
        return 0;
    }
    if (p.positionalArguments().isEmpty())
        p.showHelp(1);

    QString err;
    const QString message = p.positionalArguments().join(' ');
    const auto sym = WsprEncode::encodeMessage(message, &err);
    if (!sym) {
        std::fprintf(stderr, "Cannot encode \"%s\": %s\n", qPrintable(message), qPrintable(err));
        return 1;
    }
    WsprModulator::Params mp;
    mp.offsetHz = qBound(0, p.value("offset").toInt(), 200);
    mp.testTones = p.isSet("test");
    if (p.isSet("cpfsk"))
        mp.shaping = WsprModulator::Shaping::ContinuousPhase;
    const float volume = p.value("volume").toFloat();
    int remaining = qMax(1, p.value("repeat").toInt());

    WsprAudioOut out;
    out.setDevice(p.value("device"));
    std::printf("Message \"%s\", audio %.1f-%.1f Hz, device: %s\n", qPrintable(message.toUpper()),
                1400.0 + mp.offsetHz, 1400.0 + mp.offsetHz + (mp.testTones ? 300 : 4.4),
                p.isSet("device") ? qPrintable(p.value("device")) : "default");

    QTimer startTimer;
    startTimer.setSingleShot(true);
    startTimer.setTimerType(Qt::PreciseTimer);

    auto scheduleNext = [&] {
        if (p.isSet("now")) {
            startTimer.start(0);
            return;
        }
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QDateTime start(now.date(), QTime(now.time().hour(), now.time().minute() - now.time().minute() % 2, 1),
                        QTimeZone::utc());
        while (start <= now.addMSecs(200))
            start = start.addSecs(120);
        std::printf("Waiting for %s UTC ...\n", qPrintable(start.toString("HH:mm:ss")));
        startTimer.start(int(now.msecsTo(start)));
    };

    QObject::connect(&startTimer, &QTimer::timeout, [&] {
        std::printf("%s UTC  transmitting (%d left after this)\n",
                    qPrintable(QDateTime::currentDateTimeUtc().toString("HH:mm:ss.zzz")), remaining - 1);
        if (!out.start(*sym, mp, volume)) {
            std::fprintf(stderr, "Audio error: %s\n", qPrintable(out.lastError()));
            app.exit(1);
        }
    });
    QObject::connect(&out, &WsprAudioOut::finished, [&] {
        std::printf("%s UTC  finished (%.1f s played)%s\n",
                    qPrintable(QDateTime::currentDateTimeUtc().toString("HH:mm:ss")), out.playedSeconds(),
                    out.lastError().isEmpty() ? "" : qPrintable(" - " + out.lastError()));
        if (--remaining > 0)
            scheduleNext();
        else
            app.quit();
    });

    scheduleNext();
    return app.exec();
}
