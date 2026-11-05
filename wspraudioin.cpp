#include "wspraudioin.h"

#include <QAudioSource>
#include <QMediaDevices>
#include <QRegularExpression>
#include <QtEndian>
#include <cmath>

namespace {
QString formatText(const QAudioFormat &f)
{
    QString s;
    switch (f.sampleFormat()) {
    case QAudioFormat::UInt8: s = "8-bit"; break;
    case QAudioFormat::Int16: s = "16-bit"; break;
    case QAudioFormat::Int32: s = "32-bit"; break;
    case QAudioFormat::Float: s = "float"; break;
    default: s = "?"; break;
    }
    return QString("%1 Hz, %2 ch, %3").arg(f.sampleRate()).arg(f.channelCount()).arg(s);
}
} // namespace

WsprAudioIn::WsprAudioIn(QObject *parent) : QObject(parent)
{
    m_device = QMediaDevices::defaultAudioInput();
    m_watchdog.setInterval(1000);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        if (!m_source)
            return;
        // C# trusted the device; here a device that goes quiet ends the recording with a reason
        static constexpr int kSilentMs = 5000;
        if (m_io && m_io->bytesAvailable() > 0)
            onReadyRead();
        const qint64 now = m_wav.samplesWritten();
        const qint64 last = property("lastCount").toLongLong();
        const int quietMs = property("quietMs").toInt();
        if (now == last) {
            setProperty("quietMs", quietMs + 1000);
            if (quietMs + 1000 >= kSilentMs) {
                if (now >= m_target * 9 / 10)
                    finish(true, QString());
                else
                    finish(false, now == 0 ? "no audio from " + m_device.description()
                                           : "audio stopped after " + QString::number(now / 12000) + " s");
            }
        } else {
            setProperty("quietMs", 0);
            setProperty("lastCount", now);
        }
    });
}

WsprAudioIn::~WsprAudioIn()
{
    if (m_source) {
        m_source->stop();
        m_wav.close();
    }
}

QStringList WsprAudioIn::inputDeviceNames()
{
    QStringList names;
    for (const QAudioDevice &d : QMediaDevices::audioInputs())
        names << d.description();
    return names;
}

bool WsprAudioIn::setDevice(const QString &name)
{
    m_device = QMediaDevices::defaultAudioInput();
    QString n = name.trimmed();
    if (n.isEmpty())
        return true;
    const auto inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice &d : inputs)
        if (d.description() == n) {
            m_device = d;
            return true;
        }
    // C# list text: "1: Microphone (USB Audio Device)"; WinMM names stop at 31 characters
    n.remove(QRegularExpression("^\\d+:\\s*"));
    for (const QAudioDevice &d : inputs)
        if (d.description().compare(n, Qt::CaseInsensitive) == 0) {
            m_device = d;
            return true;
        }
    if (n.size() >= 8)
        for (const QAudioDevice &d : inputs)
            if (d.description().startsWith(n, Qt::CaseInsensitive)) {
                m_device = d;
                return true;
            }
    return false;
}

bool WsprAudioIn::start(const QString &wavPath, int lengthMs, const QDateTime &fileStartUtc)
{
    if (m_source) {
        m_error = "already recording";
        return false;
    }
    m_error.clear();
    m_peak = 0.0;
    m_carry.clear();
    if (m_device.isNull()) {
        m_error = "No audio input device";
        return false;
    }
    QAudioFormat want;
    want.setSampleRate(12000);
    want.setChannelCount(1);
    want.setSampleFormat(QAudioFormat::Int16);
    const QAudioFormat fmt = m_device.isFormatSupported(want) ? want : m_device.preferredFormat();
    if (!fmt.isValid()) {
        m_error = "Cannot find a recording format for " + m_device.description();
        return false;
    }
    m_formatText = formatText(fmt);
    if (!m_wav.open(wavPath, 12000)) {
        m_error = "Cannot write " + wavPath + ": " + m_wav.errorString();
        return false;
    }
    m_path = wavPath;
    m_target = qint64(lengthMs) * 12000 / 1000;
    m_fileStart = fileStartUtc;
    m_aligned = !fileStartUtc.isValid();
    m_skip = 0;
    m_resampler = std::make_unique<Resampler12k>(fmt.sampleRate());
    m_source = std::make_unique<QAudioSource>(m_device, fmt);
    m_source->setBufferSize(fmt.bytesForDuration(500000));   // 0.5 s
    m_io = m_source->start();
    if (!m_io || m_source->error() == QAudio::OpenError) {
        m_error = "Cannot open " + m_device.description();
        m_source.reset();
        m_io = nullptr;
        m_wav.close();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &WsprAudioIn::onReadyRead);
    setProperty("quietMs", 0);
    setProperty("lastCount", 0);
    m_watchdog.start();
    return true;
}

void WsprAudioIn::onReadyRead()
{
    if (!m_source || !m_io)
        return;
    const QAudioFormat fmt = m_source->format();
    const int ch = qMax(1, fmt.channelCount());
    const int bps = fmt.bytesPerSample();
    const int frameBytes = bps * ch;
    QByteArray data = m_carry + m_io->readAll();
    const int frames = data.size() / frameBytes;
    m_carry = data.mid(frames * frameBytes);
    if (frames == 0)
        return;

    QVector<float> mono(frames);
    const char *p = data.constData();
    for (int i = 0; i < frames; ++i) {
        double acc = 0;
        for (int c = 0; c < ch; ++c, p += bps) {
            switch (fmt.sampleFormat()) {
            case QAudioFormat::UInt8: acc += (quint8(*p) - 128) / 128.0; break;
            case QAudioFormat::Int16: acc += qFromLittleEndian<qint16>(p) / 32768.0; break;
            case QAudioFormat::Int32: acc += qFromLittleEndian<qint32>(p) / 2147483648.0; break;
            case QAudioFormat::Float: acc += qFromLittleEndian<float>(p); break;
            default: break;
            }
        }
        mono[i] = float(acc / ch);
    }
    QVector<float> out;
    m_resampler->process(mono.constData(), frames, out);
    if (!m_aligned) {
        // the first sample of this buffer was captured about one buffer-length ago
        m_aligned = true;
        const qint64 bufMs = qint64(frames) * 1000 / qMax(1, fmt.sampleRate());
        const QDateTime first = QDateTime::currentDateTimeUtc().addMSecs(-bufMs);
        const qint64 offsetMs = m_fileStart.msecsTo(first);
        if (offsetMs > 0) {   // started late: silence for the part of the cycle that was missed
            const QVector<float> pad(int(qMin<qint64>(offsetMs * 12, m_target)), 0.0f);
            m_wav.write(pad.constData(), pad.size(), 1.0f);
        } else {
            m_skip = -offsetMs * 12;
        }
        setProperty("alignMs", offsetMs);
    }
    int from = 0;
    if (m_skip > 0) {
        from = int(qMin<qint64>(m_skip, out.size()));
        m_skip -= from;
    }
    qint64 n = qMin<qint64>(out.size() - from, m_target - m_wav.samplesWritten());
    if (n > 0) {
        for (qint64 i = from; i < from + n; ++i)
            m_peak = qMax(m_peak, double(qMin(1.0f, std::fabs(out[i] * m_gain))));
        m_wav.write(out.constData() + from, int(n), m_gain);
    }
    if (m_wav.samplesWritten() >= m_target)
        finish(true, QString());
}

void WsprAudioIn::stop()
{
    if (m_source)
        finish(m_wav.samplesWritten() > 0, m_wav.samplesWritten() > 0 ? QString() : "stopped");
}

void WsprAudioIn::finish(bool ok, const QString &error)
{
    if (!m_source)
        return;
    m_watchdog.stop();
    if (m_io)
        m_io->disconnect(this);
    QAudioSource *src = m_source.release();   // may be inside its own readyRead: delete later
    src->stop();
    src->deleteLater();
    m_io = nullptr;
    const bool written = m_wav.close();
    m_resampler.reset();
    if (!written && ok) {
        ok = false;
        m_error = "Cannot finish " + m_path;
    } else {
        m_error = error;
    }
    emit finished(m_path, ok, m_error);
}

// ---- InputLevelMonitor --------------------------------------------------------------------

InputLevelMonitor::InputLevelMonitor(QObject *parent) : QObject(parent) {}
InputLevelMonitor::~InputLevelMonitor() { stop(); }

bool InputLevelMonitor::start(const QString &deviceName)
{
    stop();
    WsprAudioIn finder;
    if (!finder.setDevice(deviceName) && !deviceName.isEmpty()) {
        m_error = "No input called " + deviceName;
        return false;
    }
    QAudioDevice dev;
    for (const QAudioDevice &d : QMediaDevices::audioInputs())
        if (d.description() == finder.deviceName())
            dev = d;
    if (dev.isNull())
        dev = QMediaDevices::defaultAudioInput();
    QAudioFormat fmt = dev.preferredFormat();
    m_source = std::make_unique<QAudioSource>(dev, fmt);
    m_source->setBufferSize(fmt.bytesForDuration(100000));
    m_io = m_source->start();
    if (!m_io) {
        m_error = "Cannot open " + dev.description();
        m_source.reset();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, [this] {
        if (!m_source || !m_io)
            return;
        const QAudioFormat f = m_source->format();
        const QByteArray d = m_io->readAll();
        const int bps = f.bytesPerSample();
        if (bps <= 0)
            return;
        float peak = 0;
        for (int i = 0; i + bps <= d.size(); i += bps) {
            const char *p = d.constData() + i;
            float v = 0;
            switch (f.sampleFormat()) {
            case QAudioFormat::UInt8: v = (quint8(*p) - 128) / 128.0f; break;
            case QAudioFormat::Int16: v = qFromLittleEndian<qint16>(p) / 32768.0f; break;
            case QAudioFormat::Int32: v = float(qFromLittleEndian<qint32>(p) / 2147483648.0); break;
            case QAudioFormat::Float: v = qFromLittleEndian<float>(p); break;
            default: break;
            }
            peak = qMax(peak, std::fabs(v) * m_gain);
        }
        emit level(qMin(1.0f, peak));
    });
    return true;
}

void InputLevelMonitor::stop()
{
    if (!m_source)
        return;
    if (m_io)
        m_io->disconnect(this);
    QAudioSource *s = m_source.release();
    s->stop();
    s->deleteLater();
    m_io = nullptr;
    emit level(0.0f);
}
