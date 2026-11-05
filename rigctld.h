#pragma once
// Starting and stopping Hamlib's rigctld, and the rig list (C#: Form1 runRigCtlD / startRigCtlD /
// startRigCtlDLinux / stopRigCtlD / checkRigctld / getRigList / readRigctl / SaveRigctl).
// Talking to rigctld once it runs is in rigctl.h.
//
// Differences from the C# version:
//  - rigctld is started directly with an argument list and only the model number ("-m 1046").
//    C# passed the whole rig-list line ("-m  1046  Yaesu  FT-450 ...") and relied on rigctld
//    ignoring the extra words;
//  - "running" means rigctld answers on its TCP port, not that some process called rigctld exists
//    (C# could report "running" for a copy that had failed to open the COM port, or one started by
//    another program on a different port);
//  - Stop ends the copy this program started; only if it didn't start one does it fall back to the
//    C# behaviour of ending every rigctld (so one left over from the C# app can still be stopped);
//  - the same code runs on Windows and Linux (C# had two copies, the Linux one adding " &").
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

struct RigctlSettings          // wspr.rigctl (one row, RigctlID 0)
{
    QString radio;             // line from "rigctl -l", e.g. " 1046  Yaesu  FT-450  ..." (as C# stored it)
    QString comPort;           // "COM3", "/dev/ttyUSB0"
    QString baud = "4800";
    QString ipv4 = "127.0.0.1";
    QString port = "4532";
    bool noRigctld = false;    // "Do not use rigctld"
    bool vox = false;          // "Use VOX to start TX"
    double ppm = 0.0;          // rig reference correction: tune this many ppm high (new; column "ppm")
};

// Rig reference correction. A rig whose reference runs k ppm low puts every signal k ppm low, so it is
// tuned k ppm high: dial = wanted * (1 + k/1e6). Reading the dial back gives the frequency actually received.
qint64 correctedDialHz(qint64 wantedHz, double ppm);
qint64 actualFromDialHz(qint64 dialHz, double ppm);

namespace Hamlib {

// rigctl / rigctld (.exe on Windows) in dir, else on PATH. Empty if not found.
QString executable(const QString &dir, const QString &name);
// Model number from a rig-list line or a bare number: " 1046  Yaesu FT-450 ..." -> 1046; 0 if none
int modelNumber(const QString &radio);
// "rigctl -l" output -> the lines that describe a rig (header and blank lines dropped)
QStringList parseRigList(const QString &output);
// The rigctld address is this PC (127.x, localhost, ::1, 0.0.0.0 or one of its own addresses). If not,
// rigctld runs on another PC: it is only connected to, never started here.
bool isLocalAddress(const QString &ipOrHost);
// rigctld arguments: -m model -r port -s baud -T ip -t tcpport
QStringList rigctldArguments(const RigctlSettings &s);
// Runs "rigctl -l" (up to 15 s). Empty list + error if rigctl can't be run.
QStringList rigList(const QString &hamlibDir, QString *error = nullptr);

} // namespace Hamlib

class RigctldProcess : public QObject
{
    Q_OBJECT
public:
    explicit RigctldProcess(QObject *parent = nullptr);
    ~RigctldProcess() override;   // stops the copy it started (C#: stopRigCtlD(true) on closing)

    // Starts rigctld (stopping one this object started earlier). false + error() if it can't start.
    bool start(const QString &hamlibDir, const RigctlSettings &s);
    // Stops the copy started here; if none, ends every rigctld (C# behaviour). Returns true if one was stopped.
    bool stop();
    bool startedHere() const;
    // rigctld answers "f" at ip:port within timeoutMs
    static bool isAnswering(const QString &ip, int port, int timeoutMs = 1000);
    QString error() const { return m_error; }
    QString lastOutput() const { return m_output; }   // what rigctld printed (useful when it exits at once)

signals:
    void exited(int code, const QString &output);    // the copy started here stopped by itself

private:
    QProcess *m_proc = nullptr;
    QString m_error;
    QString m_output;
    bool m_stopping = false;
};
