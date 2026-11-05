#pragma once
// WSPR 4-FSK audio generator - port of FSK.cs (FSKMod), without the audio output part
// (that is WsprAudioOut). Pure maths, no Qt Multimedia, so it can be tested and written to WAV.
//
// Two shapings, which measure the same on air (same splatter to 0.3 dB, same decode limit
// with WSJT-X wsprd at about -30 dB - see PORTING_NOTES.md):
//   Legacy (default) - exactly what FSK.cs does: sin(2*pi*f*t) with t counted from the start of
//                      the transmission, tone spacing 1.46413 Hz, 8196-sample (0.683 s) symbols at
//                      12 kHz, each symbol multiplied by the "alpha" window. Because spacing x symbol
//                      length is ~1.000001, the phase is effectively continuous at tone changes
//                      (in test-tone mode, 100 Hz spacing, it is not - that mode clicks).
//   ContinuousPhase  - phase accumulator, exact WSPR timing (8192 samples at 12 kHz, spacing
//                      12000/8192 = 1.4648 Hz, 110.59 s), constant amplitude, 10 ms raised-cosine
//                      ramp at the start and end of the transmission only. Click-free test tones.
#include <QByteArray>
#include <QVector>
#include <array>
#include <cstdint>

class WsprModulator
{
public:
    enum class Shaping { Legacy, ContinuousPhase };

    struct Params
    {
        int offsetHz = 50;            // slot offset, 0..200 (tone 0 = 1400 + offset)
        double baseHz = 1400.0;
        bool testTones = false;       // C# "test" flag: 100 Hz spacing so the tones are audible
        double alpha = 0.1;           // C# Gaussian window alpha (Legacy only)
        int sampleRate = 12000;       // any rate; output is computed, not resampled
        Shaping shaping = Shaping::Legacy;
        double rampMs = 10.0;         // ContinuousPhase: fade in/out of the whole transmission
    };

    static constexpr int kSymbols = 162;

    WsprModulator(const std::array<uint8_t, kSymbols> &symbols, const Params &p);

    double toneSpacing() const;
    double toneHz(int tone) const { return m_p.baseHz + m_p.offsetHz + toneSpacing() * tone; }
    qint64 totalSamples() const { return m_total; }
    double durationSeconds() const { return double(m_total) / m_p.sampleRate; }
    const Params &params() const { return m_p; }

    // Streaming: fills up to n samples (-1..1), returns how many were written (0 at the end)
    int generate(float *out, int n);
    void rewind();
    qint64 position() const { return m_pos; }

    // Whole transmission as 16-bit mono PCM WAV (C#: RecordWSPR builds the same header)
    QByteArray toWav();
    QVector<float> renderAll();

private:
    float sampleAt(qint64 n);

    std::array<uint8_t, kSymbols> m_sym;
    Params m_p;
    qint64 m_samplesPerSymbol = 0;   // Legacy
    double m_symbolSamples = 0.0;    // ContinuousPhase (fractional)
    qint64 m_total = 0;
    qint64 m_pos = 0;
    double m_phase = 0.0;
    QVector<double> m_window;        // Legacy alpha window, one symbol long
};
