#pragma once
// Plays a WSPR transmission on a sound card - replaces the NAudio part of FSK.cs
// (PlayWSPR / RunAudio) and Form1.MonitorFSK.
//
// Differences from the C# version:
//  - samples are generated as the sound card asks for them (no 2.6 MB WAV built in memory first);
//  - generated at the device's own sample rate/format, so no resampling;
//  - setVolume() takes effect immediately (C#: Form1.MonitorFSK wrote volumeFactor in a loop, but the
//    line copying it to waveOut.Volume was commented out, so changes during a transmission were lost);
//  - no busy-wait: C# MonitorFSK spun a CPU core for the whole transmission;
//  - finished() is emitted when the last sample has played or stop() is called.
#include "wsprmodulator.h"

#include <QAudioDevice>
#include <QIODevice>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

class QAudioSink;

// QIODevice that renders the modulator in the requested audio format (pull mode)
class WsprToneSource : public QIODevice
{
    Q_OBJECT
public:
    WsprToneSource(std::unique_ptr<WsprModulator> mod, const QAudioFormat &fmt, QObject *parent = nullptr);
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;
    bool atEnd() const override;
    const WsprModulator &modulator() const { return *m_mod; }

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    std::unique_ptr<WsprModulator> m_mod;
    QAudioFormat m_fmt;
    QVector<float> m_buf;
};

class WsprAudioOut : public QObject
{
    Q_OBJECT
public:
    explicit WsprAudioOut(QObject *parent = nullptr);
    ~WsprAudioOut() override;

    static QStringList outputDeviceNames();                 // C#: findSound (output side)
    void setDevice(const QString &name);                    // empty = system default

    // C#: new FSKMod(symbols, offset, test, alpha, volume). Returns false if the device can't open.
    bool start(const std::array<uint8_t, WsprModulator::kSymbols> &symbols, WsprModulator::Params params,
               float volume);
    void stop();                                            // C#: stopPlay = true
    void setVolume(float v);                                // 0..1, applies immediately
    bool isPlaying() const;
    QString lastError() const { return m_error; }
    double playedSeconds() const;

signals:
    void finished();

private:
    void cleanup();

    QAudioDevice m_device;
    std::unique_ptr<QAudioSink> m_sink;
    WsprToneSource *m_source = nullptr;
    float m_volume = 1.0f;
    double m_lastPlayed = 0.0;
    QString m_error;
};
