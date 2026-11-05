#pragma once
// One receive cycle: record ~110 s from the sound card, then run wsprd on it while the next cycle
// records (C#: RXForm.Start_Receive / Record_Decode / DecodeAndSave / runDecoder).
//
// Same sequence as the C# version: three rotating files temp1..3.wav in the data folder, a new
// recording is refused while one is running, and a decode waits for the previous one to finish.
// Differences:
//  - the dial frequency used for "-f", the "nil rcvd" row and the upload is the one at the END of
//    the recording (C# used Frequency at decode time for -f but prevFreq, taken at :48, for the rest);
//  - a recording that fails is reported (statusChanged/problem) instead of being decoded as empty;
//  - wsprd is checked for before recording, and its run is limited to 100 s.
#include "rxspot.h"

#include <QDateTime>
#include <QObject>
#include <optional>

class WsprAudioIn;
class WsprdRunner;

class RxCycle : public QObject
{
    Q_OBJECT
public:
    struct Options { bool deep = false; bool quick = false; int osd = 0; };

    explicit RxCycle(QObject *parent = nullptr);
    ~RxCycle() override;

    void setDataDir(const QString &dir);
    QString dataDir() const { return m_dataDir; }
    void setWsprdDir(const QString &dir) { m_wsprdDir = dir; }
    QString wsprdDir() const { return m_wsprdDir; }
    bool setDevice(const QString &name);
    QString deviceName() const;
    void setGain(float g);
    void setDial(const QString &mhz) { m_dial = mhz; }   // C#: set_frequency
    QString dial() const { return m_dial; }
    void setOptions(const Options &o) { m_opt = o; }
    Options options() const { return m_opt; }
    void setBlockDecodes(bool b) { m_block = b; }        // C#: blockDecodes (set while transmitting)
    bool blockDecodes() const { return m_block; }
    void setReporter(const QString &call, const QString &locator) { m_call = call; m_loc = locator; }

    // C#: Start_Receive. Records until second 110 of the cycle and then decodes.
    // false if a recording is already running, wsprd is missing or the sound card won't open.
    bool start(const QDateTime &utcNow = QDateTime::currentDateTimeUtc());
    void stop();                       // abandons the current recording (no decode)
    bool isRecording() const;
    bool isDecoding() const;
    QString currentWav() const { return m_wav; }
    void deleteRecordings();           // temp1..3.wav (C#: done in set_header)
    QString lastError() const { return m_error; }

    // Decode an existing file (used by the tests and the wsprrx tool)
    bool decodeFile(const QString &wav, const QDateTime &cycleStart, const QString &dial);

signals:
    void statusChanged(const QString &status);            // "receiving", "decoding", ...
    void recordingStarted(const QString &wav, int durationMs);
    void decoded(const Rx::DecodeResult &result, const QString &dial, const QDateTime &cycleStart);
    void problem(const QString &message);                 // shown briefly to the user

private:
    struct Job { QString wav; QDateTime cycle; QString dial; };
    void onRecorded(const QString &wav, bool ok, const QString &error);
    void runDecode(const Job &job);
    void onDecoded(bool ok, const QString &output, const QString &error);

    WsprAudioIn *m_in;
    WsprdRunner *m_runner;
    QString m_dataDir;
    QString m_wsprdDir;
    QString m_dial;
    QString m_call, m_loc;
    Options m_opt;
    bool m_block = false;
    int m_wavNo = 1;
    QString m_wav;
    QDateTime m_recCycle;
    std::optional<Job> m_running;
    std::optional<Job> m_queued;
    QString m_error;
};
