// Checks WsprModulator / WsprToneSource without a sound card: lengths, tone frequencies, and that
// demodulating the audio (Goertzel at the 4 tones) gives back the encoded symbols.
// The on-air checks (legacy output = FSK.cs within 1 LSB; both shapings decode in WSJT-X wsprd)
// were done with tools/wsprwav - see PORTING_NOTES.md.
#include "wspraudioout.h"
#include "wsprencode.h"
#include "wsprmodulator.h"

#include <QAudioFormat>
#include <QCoreApplication>
#include <cmath>
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static double goertzel(const float *x, int n, double f, int fs)
{
    const double w = 2 * 3.141592653589793 * f / fs, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

// returns number of symbols recovered correctly
static int demodulate(const QVector<float> &x, const WsprModulator &m, const WsprEncode::Symbols &sym)
{
    const int fs = m.params().sampleRate;
    const double symLen = double(m.totalSamples()) / 162;
    int good = 0;
    for (int k = 0; k < 162; ++k) {
        const int a = int(k * symLen + symLen * 0.1), n = int(symLen * 0.8);   // middle 80% of the symbol
        int best = 0; double bp = -1;
        for (int t = 0; t < 4; ++t) {
            const double p = goertzel(x.constData() + a, n, m.toneHz(t), fs);
            if (p > bp) { bp = p; best = t; }
        }
        good += best == sym[size_t(k)];
    }
    return good;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto sym = WsprEncode::encodeType1("G4GCI", "IO90", 23);
    CHECK(sym.has_value());

    for (auto shaping : {WsprModulator::Shaping::Legacy, WsprModulator::Shaping::ContinuousPhase}) {
        for (int fs : {12000, 48000}) {
            WsprModulator::Params p;
            p.offsetHz = 50;
            p.sampleRate = fs;
            p.shaping = shaping;
            WsprModulator m(*sym, p);
            const bool legacy = shaping == WsprModulator::Shaping::Legacy;
            if (fs == 12000)
                CHECK(m.totalSamples() == (legacy ? 8196 * 162 : 8192 * 162));   // FSK.cs: (int)(12000*0.683)
            CHECK(std::abs(m.toneHz(0) - 1450.0) < 1e-9);
            CHECK(std::abs(m.toneSpacing() - (legacy ? 1.46413 : 12000.0 / 8192.0)) < 1e-9);
            const QVector<float> x = m.renderAll();
            float peak = 0;
            for (float v : x) peak = std::max(peak, std::abs(v));
            CHECK(peak <= 1.0f && peak > 0.99f);
            const int good = demodulate(x, m, *sym);
            std::printf("     %-15s %5d Hz: %.2f s, %d/162 symbols recovered\n", legacy ? "legacy" : "continuous-phase",
                        fs, m.durationSeconds(), good);
            CHECK(good == 162);
        }
    }

    WsprModulator::Params t;
    t.testTones = true;
    CHECK(std::abs(WsprModulator(*sym, t).toneHz(3) - (1400 + 50 + 300)) < 1e-9);   // C# test mode: 100 Hz spacing

    // WAV header as written by FSK.cs RecordWSPR
    WsprModulator w(*sym, WsprModulator::Params{});
    const QByteArray wav = w.toWav();
    CHECK(wav.left(4) == "RIFF" && wav.mid(8, 8) == "WAVEfmt " && wav.mid(36, 4) == "data");
    CHECK(wav.size() == 44 + 8196 * 162 * 2);

    // WsprToneSource: stereo float and stereo int16 frames carry the same sample on both channels
    for (auto sf : {QAudioFormat::Float, QAudioFormat::Int16}) {
        QAudioFormat fmt;
        fmt.setSampleRate(48000);
        fmt.setChannelCount(2);
        fmt.setSampleFormat(sf);
        WsprModulator::Params p;
        p.sampleRate = 48000;
        WsprModulator ref(*sym, p);
        WsprToneSource src(std::make_unique<WsprModulator>(*sym, p), fmt);
        CHECK(src.bytesAvailable() == ref.totalSamples() * fmt.bytesPerFrame());
        qint64 total = 0;
        bool same = true;
        QVector<float> r(4096);
        while (!src.atEnd()) {
            const QByteArray b = src.read(4096 * fmt.bytesPerFrame());
            const int frames = int(b.size() / fmt.bytesPerFrame());
            ref.generate(r.data(), frames);
            for (int i = 0; i < frames && same; ++i) {
                for (int c = 0; c < 2; ++c) {
                    const char *pch = b.constData() + i * fmt.bytesPerFrame() + c * fmt.bytesPerSample();
                    float v;
                    if (sf == QAudioFormat::Float) std::memcpy(&v, pch, 4);
                    else { qint16 s; std::memcpy(&s, pch, 2); v = s / 32760.0f; }
                    same = same && std::abs(v - r[i]) < 1e-4f;
                }
            }
            total += frames;
        }
        CHECK(same && total == ref.totalSamples());
    }

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
