#pragma once
// Pieces of the recorder that do not need a sound card, so they can be tested:
//  - Resampler12k: any input rate -> 12000 Hz mono (wsprd only reads 12 kHz files).
//    C# asked NAudio for 12000 Hz directly; Qt on Windows (WASAPI) usually only offers the
//    device's own rate (44.1/48 kHz), so the conversion is done here.
//  - WavWriter: 16-bit mono WAV, header patched on close.
#include <QFile>
#include <QString>
#include <QVector>

class Resampler12k
{
public:
    explicit Resampler12k(int inputRate);
    int inputRate() const { return m_inRate; }
    // Appends 12 kHz samples to out
    void process(const float *in, int n, QVector<float> &out);

private:
    int m_inRate;
    double m_step;              // input samples per output sample
    QVector<float> m_taps;      // low-pass, cut-off 5 kHz (input rate), empty when inRate == 12000
    QVector<float> m_hist;      // last taps.size()-1 input samples
    QVector<float> m_filtered;  // filtered input not yet consumed
    double m_pos = 0.0;         // position of the next output sample in m_filtered
};

class WavWriter
{
public:
    ~WavWriter();
    bool open(const QString &path, int sampleRate = 12000);
    // gain applied, clipped to +-1, then 16-bit (C#: RecordLineInAsync_Gain)
    void write(const float *samples, int n, float gain = 1.0f);
    qint64 samplesWritten() const { return m_samples; }
    bool close();               // writes the final sizes into the header
    bool isOpen() const { return m_file.isOpen(); }
    QString errorString() const { return m_file.errorString(); }

private:
    QFile m_file;
    int m_rate = 12000;
    qint64 m_samples = 0;
};
