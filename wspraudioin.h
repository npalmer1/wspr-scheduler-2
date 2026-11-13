#pragma once
// Records one WSPR cycle from a sound card to a 12 kHz mono WAV for wsprd
// (C#: RXForm.RecordLineInAsync_Gain, NAudio WaveInEvent).
//
// Differences from the C# version:
//  - records in whatever format the device offers and converts to 12 kHz mono 16-bit
//    (C# required the device to accept 12 kHz directly);
//  - stops after exactly the requested number of samples rather than after a Task.Delay, so a busy
//    UI thread cannot make the file longer or shorter;
//  - the file is lined up with the start of the cycle (even minute), whether recording began at :59,
//    :00, :01 or the sound card took a moment to start; C#'s dt was off by up to a second either way;
//  - a device that fails or delivers nothing gives finished(ok = false) with a reason, instead of an
//    empty file that the decoder then skipped silently;
//  - the device is chosen by name, and old C# names ("1: Microphone (USB Audio Device)", truncated
//    to 31 characters by Windows' old API) still match.
#include <QAudioDevice>
#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>

#include "wavrecorder.h"

class QAudioSource;
class QIODevice;

class WsprAudioIn : public QObject
{
    Q_OBJECT
public:
    explicit WsprAudioIn(QObject *parent = nullptr);
    ~WsprAudioIn() override;

    static QStringList inputDeviceNames();
    // Exact name, then the C# form "N: name" / a name Windows cut short. Empty = default input.
    // Returns false (and keeps the default) if nothing matches.
    bool setDevice(const QString &name);
    QString deviceName() const { return m_device.description(); }

    void setGain(float g) { m_gain = g; }    // applies immediately (C#: gain field read per buffer)
    float gain() const { return m_gain; }

    // Records lengthMs of audio. With a valid fileStartUtc the file is aligned to that moment: audio from
    // before it is dropped and a late start is padded with silence, so sample 0 is fileStartUtc
    // (wsprd measures dt from the start of the file). Without one, the file starts with the first sample.
    bool start(const QString &wavPath, int lengthMs, const QDateTime &fileStartUtc = QDateTime());
    void stop();                              // finishes the file early (ok = true if anything recorded)
    bool isRecording() const { return m_source != nullptr; }
    QString lastError() const { return m_error; }
    QString formatDescription() const { return m_formatText; }   // e.g. "48000 Hz, 2 ch, float"
    double peakLevel() const { return m_peak; }                 // 0..1 of the last recording, after gain

signals:
    void finished(const QString &wavPath, bool ok, const QString &error);

private:
    void onReadyRead();
    void finish(bool ok, const QString &error);

    QAudioDevice m_device;
    std::unique_ptr<QAudioSource> m_source;
    QIODevice *m_io = nullptr;
    std::unique_ptr<Resampler12k> m_resampler;
    WavWriter m_wav;
    QString m_path;
    qint64 m_target = 0;          // 12 kHz samples wanted
    float m_gain = 1.0f;
    double m_peak = 0.0;
    QString m_error;
    QString m_formatText;
    QTimer m_watchdog;
    QByteArray m_carry;           // partial frame left over from the last read
    QDateTime m_fileStart;        // wall-clock time of sample 0 (invalid = not aligned)
    bool m_aligned = false;       // first buffer seen and offset applied
    qint64 m_skip = 0;            // 12 kHz samples still to drop (recording began before m_fileStart)
};

// Peak level of an input, for the "Monitor mic/line" meter on the RX & Sound tab (C#: Form1.MonitorMic).
// Gain is applied as the recorder applies it, so the meter shows what wsprd will get.
class InputLevelMonitor : public QObject
{
    Q_OBJECT
public:
    explicit InputLevelMonitor(QObject *parent = nullptr);
    ~InputLevelMonitor() override;
    bool start(const QString &deviceName);   // same name matching as WsprAudioIn::setDevice
    void stop();
    bool isRunning() const { return m_source != nullptr; }
    void setGain(float g) { m_gain = g; }
    QString lastError() const { return m_error; }

signals:
    void level(float peak);                   // 0..1 after gain: the loudest point of each 100 ms
                                              // (C#: NAudio WaveInEvent's 100 ms buffers)

private:
    std::unique_ptr<QAudioSource> m_source;
    QIODevice *m_io = nullptr;
    float m_gain = 1.0f;
    float m_windowPeak = 0.0f;   // loudest so far in the current 100 ms
    QElapsedTimer m_window;
    QString m_error;
};
