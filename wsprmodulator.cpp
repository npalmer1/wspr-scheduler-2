#include "wsprmodulator.h"

#include <QDataStream>
#include <QIODevice>
#include <cmath>

namespace {
constexpr double kPi = 3.141592653589793;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kWsprSymbolSeconds = 8192.0 / 12000.0;   // 0.682667 s
constexpr double kLegacySymbolSeconds = 0.683;            // FSK.cs SymbolDuration
constexpr double kLegacySpacing = 1.46413;                // FSK.cs modFreq
} // namespace

WsprModulator::WsprModulator(const std::array<uint8_t, kSymbols> &symbols, const Params &p)
    : m_sym(symbols), m_p(p)
{
    for (auto &s : m_sym)
        s &= 3;
    if (m_p.shaping == Shaping::Legacy) {
        // FSK.cs: samplesPerSymbol = (int)(SampleRate * 0.683); window from CreateGaussianFilter
        m_samplesPerSymbol = qint64(m_p.sampleRate * kLegacySymbolSeconds);
        m_total = m_samplesPerSymbol * kSymbols;
        m_window.resize(int(m_samplesPerSymbol));
        const double len = double(m_samplesPerSymbol);
        for (int i = 0; i < m_window.size(); ++i) {
            const double t = (i - len / 2.0) / len;
            m_window[i] = std::exp(-0.5 * std::pow(kTwoPi * m_p.alpha * t, 2));
        }
    } else {
        m_symbolSamples = m_p.sampleRate * kWsprSymbolSeconds;
        m_total = qint64(std::llround(m_symbolSamples * kSymbols));
    }
}

double WsprModulator::toneSpacing() const
{
    if (m_p.testTones)
        return 100.0;
    return m_p.shaping == Shaping::Legacy ? kLegacySpacing : 12000.0 / 8192.0;
}

void WsprModulator::rewind()
{
    m_pos = 0;
    m_phase = 0.0;
}

float WsprModulator::sampleAt(qint64 n)
{
    if (m_p.shaping == Shaping::Legacy) {
        const qint64 symbol = n / m_samplesPerSymbol;
        const double f = toneHz(m_sym[size_t(symbol)]);
        const double t = double(n) / m_p.sampleRate;   // global time -> phase jumps at tone changes
        return float(std::sin(kTwoPi * f * t) * m_window[int(n % m_samplesPerSymbol)]);
    }

    const int symbol = qMin(kSymbols - 1, int(double(n) / m_symbolSamples));
    const double f = toneHz(m_sym[size_t(symbol)]);
    const float s = float(std::sin(m_phase));
    m_phase += kTwoPi * f / m_p.sampleRate;           // continuous phase
    if (m_phase > kTwoPi)
        m_phase -= kTwoPi;

    // raised-cosine fade at the start and end of the whole transmission (no key clicks)
    const double ramp = m_p.rampMs * 1e-3 * m_p.sampleRate;
    double g = 1.0;
    if (ramp > 1.0) {
        if (n < ramp)
            g = 0.5 - 0.5 * std::cos(kPi * n / ramp);
        else if (n > m_total - 1 - ramp)
            g = 0.5 - 0.5 * std::cos(kPi * double(m_total - 1 - n) / ramp);
    }
    return float(s * g);
}

int WsprModulator::generate(float *out, int n)
{
    int written = 0;
    while (written < n && m_pos < m_total)
        out[written++] = sampleAt(m_pos++);
    return written;
}

QVector<float> WsprModulator::renderAll()
{
    rewind();
    QVector<float> v(static_cast<int>(m_total));
    generate(v.data(), v.size());
    rewind();
    return v;
}

QByteArray WsprModulator::toWav()
{
    const QVector<float> s = renderAll();
    QByteArray wav;
    QDataStream w(&wav, QIODevice::WriteOnly);
    w.setByteOrder(QDataStream::LittleEndian);
    const quint32 dataBytes = quint32(s.size() * 2);
    w.writeRawData("RIFF", 4);
    w << quint32(36 + dataBytes);
    w.writeRawData("WAVE", 4);
    w.writeRawData("fmt ", 4);
    w << quint32(16) << quint16(1) << quint16(1) << quint32(m_p.sampleRate) << quint32(m_p.sampleRate * 2)
      << quint16(2) << quint16(16);
    w.writeRawData("data", 4);
    w << dataBytes;
    for (float x : s)
        w << qint16(qBound(-1.0f, x, 1.0f) * 32760.0f);   // as C#: (short)(sample * 32760), truncating
    return wav;
}
