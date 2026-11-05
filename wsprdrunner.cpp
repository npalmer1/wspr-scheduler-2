#include "wsprdrunner.h"

#include <QFileInfo>
#include <QProcess>
#include <QTimer>

WsprdRunner::WsprdRunner(QObject *parent) : QObject(parent) {}

WsprdRunner::~WsprdRunner()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

bool WsprdRunner::isRunning() const { return m_proc && m_proc->state() != QProcess::NotRunning; }

bool WsprdRunner::start(const QString &program, const QStringList &args, const QString &workingDir)
{
    if (isRunning()) {
        m_error = "wsprd is still running";
        return false;
    }
    if (program.isEmpty() || !QFileInfo::exists(program)) {
        m_error = program.isEmpty() ? "wsprd not found" : "wsprd not found: " + program;
        return false;
    }
    if (m_proc)
        m_proc->deleteLater();
    m_proc = new QProcess(this);
    m_proc->setProgram(program);
    m_proc->setArguments(args);
    m_proc->setWorkingDirectory(workingDir);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    m_timedOut = false;

    auto *limit = new QTimer(m_proc);
    limit->setSingleShot(true);
    connect(limit, &QTimer::timeout, m_proc, [this] {
        m_timedOut = true;
        m_proc->kill();
    });

    QProcess *p = m_proc;
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus st) {
        const QString out = QString::fromLocal8Bit(p->readAllStandardOutput());
        QString err;
        if (m_timedOut)
            err = QString("wsprd took longer than %1 s and was stopped").arg(m_timeoutMs / 1000);
        else if (st != QProcess::NormalExit)
            err = "wsprd crashed";
        else if (code != 0)
            err = QString("wsprd exit code %1: %2").arg(code).arg(QString::fromLocal8Bit(p->readAllStandardError()).trimmed());
        // a non-zero exit code is reported but the output is still used: the caller checks for <DecodeFinished>
        emit finished(!m_timedOut && st == QProcess::NormalExit, out, err);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit finished(false, QString(), "cannot start wsprd: " + p->errorString());
    });

    p->start();
    limit->start(m_timeoutMs);
    return true;
}

void WsprdRunner::kill()
{
    if (isRunning())
        m_proc->kill();
}
