#include "rigctld.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkInterface>

#include <algorithm>
#include <cmath>

#include "rigctl.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSerialPortInfo>

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

QString serialDevice(const QString &port)
{
    const QString p = port.trimmed();
#ifdef Q_OS_WIN
    return p;
#else
    return p.isEmpty() || p.contains('/') ? p : "/dev/" + p;
#endif
}

QString serialPortProblem(const QString &port)
{
#ifdef Q_OS_WIN
    Q_UNUSED(port);
    return QString();
#else
    const QString p = port.trimmed();
    if (p.isEmpty())
        return QString();
    static const QRegularExpression win("^COM\\d+$", QRegularExpression::CaseInsensitiveOption);
    if (win.match(p).hasMatch())
        return p + " is a Windows port name: choose this PC's port for the rig (e.g. /dev/ttyUSB0) on the "
                   "Rig control tab and Save";
    const QString dev = serialDevice(p);
    const QFileInfo fi(dev);
    if (!fi.exists())
        return dev + " not found - is the rig's USB lead plugged in? (the ports are listed again when you "
                     "open the Rig control tab)";
    if (isPhantomSerialPort(dev))
        return dev + " is not a real serial port (this PC has no port there) - choose the rig's port "
                     "(ttyUSB0, ttyACM0 ...) on the Rigctl rigs tab and Save";
    if (!fi.isReadable() || !fi.isWritable())
        return "No permission to use " + dev + ". Add yourself to the dialout group, then log out and in again:\n"
               "    sudo usermod -aG dialout $USER";
    return QString();
#endif
}

bool isPhantomSerialPort(const QString &port)
{
#ifdef Q_OS_WIN
    Q_UNUSED(port);
    return false;
#else
    const QString name = QFileInfo(port.trimmed()).fileName();
    if (!name.startsWith("ttyS"))
        return false;
    // the serial driver's port type: 0 = PORT_UNKNOWN, i.e. no UART at that address
    QFile f("/sys/class/tty/" + name + "/type");
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.readAll().trimmed() == "0";
#endif
}

QStringList serialPortNames()
{
    QStringList usb, other;
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts()) {
        const QString n = p.portName();
        if (isPhantomSerialPort(n))
            continue;
        (n.startsWith("ttyUSB") || n.startsWith("ttyACM") || p.hasVendorIdentifier() ? usb : other) << n;
    }
    auto natural = [](const QString &a, const QString &b) {   // ttyUSB2 before ttyUSB10, COM3 before COM10
        static const QRegularExpression re("^(.*?)(\\d+)$");
        const auto ma = re.match(a), mb = re.match(b);
        if (ma.hasMatch() && mb.hasMatch() && ma.captured(1) == mb.captured(1))
            return ma.captured(2).toInt() < mb.captured(2).toInt();
        return a < b;
    };
    std::sort(usb.begin(), usb.end(), natural);
    std::sort(other.begin(), other.end(), natural);
    return usb + other;
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
        a << "-r" << serialDevice(s.comPort);
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
    if (const QString why = Hamlib::serialPortProblem(s.comPort); !why.isEmpty()) {
        m_error = why;   // rigctld would only say "Permission denied" / "No such file" and stop
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
