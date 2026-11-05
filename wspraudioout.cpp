#include "wspraudioout.h"

#include <QAudioSink>
#include <QMediaDevices>
#include <cstring>

// ---------------------------------------------------------------------------
// WsprToneSource
// ---------------------------------------------------------------------------

WsprToneSource::WsprToneSource(std::unique_ptr<WsprModulator> mod, const QAudioFormat &fmt, QObject *parent)
    : QIODevice(parent), m_mod(std::move(mod)), m_fmt(fmt)
{
    open(QIODevice::ReadOnly);
}

qint64 WsprToneSource::bytesAvailable() const
{
    const qint64 frames = m_mod->totalSamples() - m_mod->position();
    return frames * m_fmt.bytesPerFrame() + QIODevice::bytesAvailable();
}

bool WsprToneSource::atEnd() const
{
    return m_mod->position() >= m_mod->totalSamples();
}

qint64 WsprToneSource::readData(char *data, qint64 maxlen)
{
    const int bpf = m_fmt.bytesPerFrame();
    const int channels = m_fmt.channelCount();
    if (bpf <= 0)
        return -1;
    const int frames = int(qMin<qint64>(maxlen / bpf, 1 << 16));
    if (m_buf.size() < frames)
        m_buf.resize(frames);
    const int n = m_mod->generate(m_buf.data(), frames);
    const int bps = m_fmt.bytesPerSample();
    for (int i = 0; i < n; ++i) {
        const float s = m_buf[i];
        for (int c = 0; c < channels; ++c) {
            char *p = data + qint64(i) * bpf + c * bps;
            switch (m_fmt.sampleFormat()) {
            case QAudioFormat::Float: std::memcpy(p, &s, 4); break;
            case QAudioFormat::Int16: { const qint16 v = qint16(s * 32760.0f); std::memcpy(p, &v, 2); break; }
            case QAudioFormat::Int32: { const qint32 v = qint32(double(s) * 2147352576.0); std::memcpy(p, &v, 4); break; }
            case QAudioFormat::UInt8: *reinterpret_cast<quint8 *>(p) = quint8(128 + int(s * 127.0f)); break;
            default: std::memset(p, 0, size_t(bps)); break;
            }
        }
    }
    return qint64(n) * bpf;   // 0 at the end -> QAudioSink goes idle
}

// ---------------------------------------------------------------------------
// WsprAudioOut
// ---------------------------------------------------------------------------

WsprAudioOut::WsprAudioOut(QObject *parent)
    : QObject(parent), m_device(QMediaDevices::defaultAudioOutput())
{
}

WsprAudioOut::~WsprAudioOut()
{
    cleanup();
}

QStringList WsprAudioOut::outputDeviceNames()
{
    QStringList names;
    for (const QAudioDevice &d : QMediaDevices::audioOutputs())
        names << d.description();
    return names;
}

void WsprAudioOut::setDevice(const QString &name)
{
    m_device = QMediaDevices::defaultAudioOutput();
    if (name.isEmpty())
        return;
    for (const QAudioDevice &d : QMediaDevices::audioOutputs())
        if (d.description() == name) {
            m_device = d;
            return;
        }
}

bool WsprAudioOut::start(const std::array<uint8_t, WsprModulator::kSymbols> &symbols, WsprModulator::Params params,
                         float volume)
{
    cleanup();
    m_error.clear();
    m_lastPlayed = 0.0;
    if (m_device.isNull()) {
        m_error = "No audio output device";
        return false;
    }
    QAudioFormat fmt = m_device.preferredFormat();
    if (!fmt.isValid() || fmt.sampleRate() < 8000) {
        fmt.setSampleRate(48000);
        fmt.setChannelCount(2);
        fmt.setSampleFormat(QAudioFormat::Int16);
    }
    params.sampleRate = fmt.sampleRate();   // generate at the device rate (tones are computed exactly)

    m_source = new WsprToneSource(std::make_unique<WsprModulator>(symbols, params), fmt, this);
    m_sink = std::make_unique<QAudioSink>(m_device, fmt);
    m_sink->setBufferSize(fmt.bytesForDuration(400000));   // 0.4 s
    setVolume(volume);
    connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State st) {
        if (!m_sink)
            return;
        if (st == QAudio::IdleState && m_source && m_source->atEnd()) {   // played to the end
            cleanup();
            emit finished();
        } else if (st == QAudio::StoppedState && m_sink->error() != QAudio::NoError) {
            m_error = QString("Audio error %1").arg(int(m_sink->error()));
            cleanup();
            emit finished();
        }
    });
    m_sink->start(m_source);
    if (m_sink->error() != QAudio::NoError) {
        m_error = QString("Cannot open \"%1\" (error %2)").arg(m_device.description()).arg(int(m_sink->error()));
        cleanup();
        return false;
    }
    return true;
}

void WsprAudioOut::stop()
{
    const bool wasPlaying = isPlaying();
    cleanup();
    if (wasPlaying)
        emit finished();
}

void WsprAudioOut::setVolume(float v)
{
    m_volume = qBound(0.0f, v, 1.0f);
    if (m_sink)
        m_sink->setVolume(m_volume);
}

bool WsprAudioOut::isPlaying() const
{
    return m_sink && (m_sink->state() == QAudio::ActiveState || m_sink->state() == QAudio::IdleState);
}

double WsprAudioOut::playedSeconds() const
{
    return m_sink ? m_sink->processedUSecs() / 1e6 : m_lastPlayed;   // last value after it stops
}

void WsprAudioOut::cleanup()
{
    if (m_sink) {
        m_lastPlayed = m_sink->processedUSecs() / 1e6;
        auto sink = std::move(m_sink);   // m_sink is null while the sink emits its final state change
        sink->stop();
    }
    if (m_source) {
        m_source->deleteLater();
        m_source = nullptr;
    }
}
