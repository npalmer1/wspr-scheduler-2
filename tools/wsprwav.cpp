// Writes a WSPR transmission to a 12 kHz WAV, e.g. to check it with WSJT-X's wsprd:
//   wsprwav "G4GCI IO90 23" 50 out.wav [cpfsk] [raw]      (default shaping = legacy, as FSK.cs)
//   wsprwav "G4GCI/P 23" 50 out.wav                          type 2
//   wsprwav "<G4GCI/P> IO90GU 23" 50 out.wav                 type 3
// Output has 1 s of silence first and is padded to 120 s, like a recorded 2-minute period.
#include "wsprencode.h"
#include "wsprmodulator.h"

#include <QCoreApplication>
#include <QFile>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList a = app.arguments();
    if (a.size() < 4) {
        std::fprintf(stderr, "usage: wsprwav \"MESSAGE\" OFFSET out.wav [cpfsk] [raw]\n");
        return 2;
    }
    QString err;
    const auto sym = WsprEncode::encodeMessage(a[1], &err);
    if (!sym) {
        std::fprintf(stderr, "encode error: %s\n", qPrintable(err));
        return 1;
    }
    WsprModulator::Params p;
    p.offsetHz = a[2].toInt();
    if (a.contains("cpfsk"))
        p.shaping = WsprModulator::Shaping::ContinuousPhase;
    WsprModulator m(*sym, p);
    QByteArray wav = m.toWav();
    if (!a.contains("raw")) {   // 1 s lead-in, pad to 120 s
        QByteArray pcm = wav.mid(44);
        pcm.prepend(QByteArray(12000 * 2, '\0'));
        pcm.append(QByteArray(qMax(0, 120 * 12000 * 2 - pcm.size()), '\0'));
        const quint32 n = quint32(pcm.size());
        auto put32 = [&](int at, quint32 v) { for (int i = 0; i < 4; ++i) wav[at + i] = char((v >> (8 * i)) & 0xff); };
        wav = wav.left(44) + pcm;
        put32(4, 36 + n);
        put32(40, n);
    }
    QFile f(a[3]);
    if (!f.open(QIODevice::WriteOnly))
        return 1;
    f.write(wav);
    std::printf("%s: %.2f s of signal, tones %.3f..%.3f Hz\n", qPrintable(a[3]), m.durationSeconds(), m.toneHz(0), m.toneHz(3));
    return 0;
}
