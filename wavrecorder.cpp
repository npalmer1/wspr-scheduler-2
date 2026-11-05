#include "wavrecorder.h"

#include <QtEndian>
#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kOutRate = 12000;
}

Resampler12k::Resampler12k(int inputRate)
    : m_inRate(inputRate > 0 ? inputRate : kOutRate), m_step(double(m_inRate) / kOutRate)
{
    if (m_inRate == kOutRate)
        return;
    // Windowed-sinc low-pass at the input rate, cut-off 5 kHz (WSPR is at 1.4-1.6 kHz; nothing
    // above 6 kHz may fold back into the 12 kHz result). Length grows with the rate ratio.
    const double fc = 5000.0 / m_inRate;   // cycles per input sample
    int n = int(std::ceil(m_step * 24)) | 1;
    n = qBound(31, n, 1023);
    m_taps.resize(n);
    const int mid = n / 2;
    double sum = 0;
    for (int i = 0; i < n; ++i) {
        const int k = i - mid;
        const double sinc = k == 0 ? 2 * fc : std::sin(2 * kPi * fc * k) / (kPi * k);
        const double w = 0.42 - 0.5 * std::cos(2 * kPi * i / (n - 1)) + 0.08 * std::cos(4 * kPi * i / (n - 1)); // Blackman
        m_taps[i] = float(sinc * w);
        sum += sinc * w;
    }
    for (float &t : m_taps)
        t = float(t / sum);
    m_hist.fill(0.0f, n - 1);
}

void Resampler12k::process(const float *in, int n, QVector<float> &out)
{
    if (m_taps.isEmpty()) {
        out.reserve(out.size() + n);
        for (int i = 0; i < n; ++i)
            out.append(in[i]);
        return;
    }
    // filter
    const int nt = m_taps.size();
    QVector<float> buf = m_hist;
    buf.reserve(buf.size() + n);
    for (int i = 0; i < n; ++i)
        buf.append(in[i]);
    for (int i = 0; i + nt <= buf.size(); ++i) {
        double acc = 0;
        const float *x = buf.constData() + i;
        for (int k = 0; k < nt; ++k)
            acc += x[k] * m_taps[k];
        m_filtered.append(float(acc));
    }
    m_hist = buf.mid(buf.size() - (nt - 1));

    // pick 12 kHz samples by linear interpolation (the signal is far below the cut-off, so this is exact enough)
    while (m_pos + 1.0 < m_filtered.size()) {
        const int i = int(m_pos);
        const double f = m_pos - i;
        out.append(float(m_filtered[i] * (1.0 - f) + m_filtered[i + 1] * f));
        m_pos += m_step;
    }
    // when downsampling, m_pos can be past the end of what is filtered so far: drop only what exists
    const int used = qMin(int(m_pos), int(m_filtered.size()));
    if (used > 0) {
        m_filtered.remove(0, used);
        m_pos -= used;
    }
}

// ---- WavWriter ----------------------------------------------------------------------------

WavWriter::~WavWriter() { close(); }

static void putHeader(QFile &f, int rate, qint64 samples)
{
    const quint32 dataBytes = quint32(samples * 2);
    char h[44];
    memcpy(h, "RIFF", 4);
    qToLittleEndian<quint32>(36 + dataBytes, h + 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    qToLittleEndian<quint32>(16, h + 16);
    qToLittleEndian<quint16>(1, h + 20);          // PCM
    qToLittleEndian<quint16>(1, h + 22);          // mono
    qToLittleEndian<quint32>(rate, h + 24);
    qToLittleEndian<quint32>(rate * 2, h + 28);
    qToLittleEndian<quint16>(2, h + 32);
    qToLittleEndian<quint16>(16, h + 34);
    memcpy(h + 36, "data", 4);
    qToLittleEndian<quint32>(dataBytes, h + 40);
    f.write(h, 44);
}

bool WavWriter::open(const QString &path, int sampleRate)
{
    close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered))   // spectrum window reads it live
        return false;
    m_rate = sampleRate;
    m_samples = 0;
    putHeader(m_file, m_rate, 0);
    return true;
}

void WavWriter::write(const float *samples, int n, float gain)
{
    if (!m_file.isOpen() || n <= 0)
        return;
    QByteArray b(n * 2, Qt::Uninitialized);
    for (int i = 0; i < n; ++i) {
        const float v = qBound(-1.0f, samples[i] * gain, 1.0f);
        const qint16 s = qint16(qBound(-32768, int(std::lround(v * 32767.0f)), 32767));
        qToLittleEndian<qint16>(s, b.data() + 2 * i);
    }
    m_file.write(b);
    m_samples += n;
}

bool WavWriter::close()
{
    if (!m_file.isOpen())
        return true;
    m_file.seek(0);
    putHeader(m_file, m_rate, m_samples);
    const bool ok = m_file.error() == QFileDevice::NoError;
    m_file.close();
    return ok;
}
