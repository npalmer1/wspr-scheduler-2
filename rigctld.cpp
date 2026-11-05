#include "rigctld.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>

#include <cmath>

#include "rigctl.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

qint64 correctedDialHz(qint64 wantedHz, double ppm)
{
    return qint64(std::llround(double(wantedHz) * (1.0 + ppm * 1e-6)));
}

qint64 actualFromDialHz(qint64 dialHz, double ppm)
{
    return qint64(std::llround(double(dialHz) / (1.0 + ppm * 1e-6)));
}

namespace Hamlib {

QString executable(const QString &dir, const QString &name)
{
#ifdef Q_OS_WIN
    const QString file = name + ".exe";
#else
    const QString file = name;
#endif
    if (!dir.trimmed().isEmpty()) {
        const QString p = QDir(dir.trimmed()).filePath(file);
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    return QStandardPaths::findExecutable(name);
}

int modelNumber(const QString &radio)
{
    static const QRegularExpression re("^\\s*(\\d+)\\b");
    const auto m = re.match(radio);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

QStringList parseRigList(const QString &output)
{
    QStringList rigs;
    for (const QString &line : output.split('\n')) {
        QString l = line;
        l.remove('\r');
        if (modelNumber(l) > 0)   // the header ("Rig #  Mfg ...") has no number
            rigs << l;
    }
    return rigs;
}

bool isLocalAddress(const QString &ipOrHost)
{
    const QString h = ipOrHost.trimmed();
    if (h.isEmpty() || h.compare("localhost", Qt::CaseInsensitive) == 0 || h == "0.0.0.0")
        return true;
    if (h.compare(QHostInfo::localHostName(), Qt::CaseInsensitive) == 0)
        return true;
    QHostAddress a(h);
    if (a.isNull())
        return false;   // another machine's name
    if (a.isLoopback())
        return true;
    for (const QHostAddress &mine : QNetworkInterface::allAddresses())
        if (mine.isEqual(a, QHostAddress::TolerantConversion))
            return true;
    return false;
}

QStringList rigctldArguments(const RigctlSettings &s)
{
    QStringList a{"-m", QString::number(modelNumber(s.radio))};
    if (!s.comPort.trimmed().isEmpty())
        a << "-r" << s.comPort.trimmed();
    if (!s.baud.trimmed().isEmpty())
        a << "-s" << s.baud.trimmed();
    a << "-T" << (s.ipv4.trimmed().isEmpty() ? QString("127.0.0.1") : s.ipv4.trimmed());
    a << "-t" << (s.port.trimmed().isEmpty() ? QString("4532") : s.port.trimmed());
    return a;
}

QStringList rigList(const QString &hamlibDir, QString *error)
{
    const QString exe = executable(hamlibDir, "rigctl");
    if (exe.isEmpty()) {
        if (error)
            *error = "rigctl not found in " + hamlibDir + " or on PATH";
        return {};
    }
    QProcess p;
    p.start(exe, {"-l"});
    if (!p.waitForStarted(5000) || !p.waitForFinished(15000)) {
        if (error)
            *error = "rigctl -l did not run: " + p.errorString();
        p.kill();
        return {};
    }
    return parseRigList(QString::fromLocal8Bit(p.readAllStandardOutput()));
}

} // namespace Hamlib

RigctldProcess::RigctldProcess(QObject *parent) : QObject(parent) {}

RigctldProcess::~RigctldProcess()
{
    if (startedHere()) {
        m_stopping = true;
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

bool RigctldProcess::startedHere() const { return m_proc && m_proc->state() != QProcess::NotRunning; }

bool RigctldProcess::start(const QString &hamlibDir, const RigctlSettings &s)
{
    m_error.clear();
    m_output.clear();
    if (Hamlib::modelNumber(s.radio) <= 0) {
        m_error = "No radio selected";
        return false;
    }
    const QString exe = Hamlib::executable(hamlibDir, "rigctld");
    if (exe.isEmpty()) {
        m_error = "rigctld not found in " + QDir::toNativeSeparators(hamlibDir) + " or on PATH";
        return false;
    }
    if (startedHere()) {
        m_stopping = true;
        m_proc->kill();
        m_proc->waitForFinished(2000);
        m_stopping = false;
    }
    if (m_proc)
        m_proc->deleteLater();
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    QProcess *p = m_proc;
    connect(p, &QProcess::readyRead, this, [this, p] {
        m_output += QString::fromLocal8Bit(p->readAll());
        if (m_output.size() > 4000)
            m_output = m_output.right(4000);
    });
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus) {
        m_output += QString::fromLocal8Bit(p->readAll());
        if (!m_stopping)
            emit exited(code, m_output.trimmed());
    });
    p->start(exe, Hamlib::rigctldArguments(s));
    if (!p->waitForStarted(5000)) {
        m_error = "Cannot start " + exe + ": " + p->errorString();
        return false;
    }
    return true;
}

bool RigctldProcess::stop()
{
    if (startedHere()) {
        m_stopping = true;
        m_proc->kill();
        m_proc->waitForFinished(3000);
        m_stopping = false;
        return true;
    }
    // C#: kill every process called rigctld (e.g. one the C# app left running)
#ifdef Q_OS_WIN
    const int rc = QProcess::execute("taskkill", {"/IM", "rigctld.exe", "/F"});
#else
    const int rc = QProcess::execute("pkill", {"-x", "rigctld"});
#endif
    return rc == 0;
}

bool RigctldProcess::isAnswering(const QString &ip, int port, int timeoutMs)
{
    RigCtl r(ip.isEmpty() ? QString("127.0.0.1") : ip, port > 0 ? port : 4532);
    r.setTimeout(timeoutMs);
    return r.isReachable();
}
