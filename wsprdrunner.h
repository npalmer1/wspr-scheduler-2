#pragma once
// Runs wsprd on one recording (C#: RXForm.runDecoder).
// QProcess with an argument list: no cmd.exe / bash in between, so paths with spaces work, and a
// time limit so a stuck decoder cannot pile up behind the next cycle (C# waited forever).
#include <QObject>
#include <QStringList>

class QProcess;

class WsprdRunner : public QObject
{
    Q_OBJECT
public:
    explicit WsprdRunner(QObject *parent = nullptr);
    ~WsprdRunner() override;

    void setTimeoutMs(int ms) { m_timeoutMs = ms; }
    bool isRunning() const;

    // false if already running or the program cannot be started (error() says why)
    bool start(const QString &program, const QStringList &args, const QString &workingDir);
    void kill();
    QString errorString() const { return m_error; }

signals:
    // ok = exited normally within the time limit; output = everything wsprd printed on stdout
    void finished(bool ok, const QString &output, const QString &error);

private:
    QProcess *m_proc = nullptr;
    int m_timeoutMs = 100000;
    QString m_error;
    bool m_timedOut = false;
};
