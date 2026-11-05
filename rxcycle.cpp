#include "rxcycle.h"

#include "wspraudioin.h"
#include "wsprdrunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

RxCycle::RxCycle(QObject *parent)
    : QObject(parent), m_in(new WsprAudioIn(this)), m_runner(new WsprdRunner(this)),
      m_dataDir(Rx::defaultDataDir())
{
    connect(m_in, &WsprAudioIn::finished, this, &RxCycle::onRecorded);
    connect(m_runner, &WsprdRunner::finished, this, &RxCycle::onDecoded);
}

RxCycle::~RxCycle() = default;

void RxCycle::setDataDir(const QString &dir) { m_dataDir = dir; }
bool RxCycle::setDevice(const QString &name) { return m_in->setDevice(name); }
QString RxCycle::deviceName() const { return m_in->deviceName(); }
void RxCycle::setGain(float g) { m_in->setGain(g); }
bool RxCycle::isRecording() const { return m_in->isRecording(); }
bool RxCycle::isDecoding() const { return m_runner->isRunning(); }

void RxCycle::deleteRecordings()
{
    for (int i = 1; i <= 3; ++i)
        QFile::remove(QDir(m_dataDir).filePath(QString("temp%1.wav").arg(i)));
}

bool RxCycle::start(const QDateTime &utcNow)
{
    m_error.clear();
    if (m_in->isRecording()) {
        Rx::debugLog("Record_Decode SKIPPED - previous cycle still running");
        m_error = "still recording the previous cycle";
        return false;
    }
    if (Rx::wsprdExecutable(m_wsprdDir).isEmpty()) {
        m_error = "Error: wsprd not found! - see RX & Sound";
        emit problem(m_error);
        return false;
    }
    if (!QDir().mkpath(m_dataDir)) {
        m_error = "Cannot create " + m_dataDir;
        emit problem(m_error);
        return false;
    }
    // don't overwrite a file that is still queued for (or being) decoded
    for (int tries = 0; tries < 3; ++tries) {
        const QString p = QDir(m_dataDir).filePath(QString("temp%1.wav").arg(m_wavNo));
        const bool busy = (m_running && m_running->wav == p) || (m_queued && m_queued->wav == p);
        if (!busy)
            break;
        m_wavNo = m_wavNo % 3 + 1;
    }
    m_wav = QDir(m_dataDir).filePath(QString("temp%1.wav").arg(m_wavNo));
    m_wavNo = m_wavNo % 3 + 1;
    QFile::remove(m_wav);

    m_recCycle = Rx::cycleStart(utcNow);
    const int ms = Rx::recordDurationMs(utcNow);
    // file = even minute + 0 .. +110 s whenever recording actually begins (C#: from whenever it began)
    if (!m_in->start(m_wav, 110000, m_recCycle)) {
        m_error = m_in->lastError();
        emit problem(m_error);
        emit statusChanged("RX error: " + m_error);
        return false;
    }
    Rx::debugLog(QString("Record_Decode START %1 for %2 ms (%3)").arg(QFileInfo(m_wav).fileName()).arg(ms)
                     .arg(m_in->formatDescription()));
    emit statusChanged("receiving");
    emit recordingStarted(m_wav, ms);
    return true;
}

void RxCycle::stop()
{
    if (!m_in->isRecording())
        return;
    m_in->disconnect(this);
    m_in->stop();
    connect(m_in, &WsprAudioIn::finished, this, &RxCycle::onRecorded);
    emit statusChanged("idle");
}

void RxCycle::onRecorded(const QString &wav, bool ok, const QString &error)
{
    if (!ok) {
        m_error = error;
        Rx::debugLog("Record_Decode END (recording failed: " + error + ")");
        emit problem("Recording failed: " + error);
        emit statusChanged("RX error: " + error);
        return;
    }
    Rx::debugLog(QString("recording aligned: first sample %1 ms after the cycle start")
                     .arg(m_in->property("alignMs").toLongLong()));
    if (QFileInfo(wav).size() <= 44) {
        Rx::debugLog("Record_Decode END (empty wav, skipped decode)");
        return;
    }
    Job job{wav, m_recCycle, m_dial};
    if (m_runner->isRunning()) {
        m_queued = job;   // C#: await _decodeTask before starting the next decode
        Rx::debugLog("Record_Decode END (decode queued behind the previous one)");
        return;
    }
    runDecode(job);
}

bool RxCycle::decodeFile(const QString &wav, const QDateTime &cycleStart, const QString &dial)
{
    if (m_runner->isRunning())
        return false;
    runDecode({wav, cycleStart, dial});
    return m_running.has_value();
}

void RxCycle::runDecode(const Job &job)
{
    if (m_block) {   // C#: DecodeAndSave returns at once while transmitting
        Rx::debugLog("decode skipped (transmitting)");
        emit statusChanged("idle/not receiving");
        return;
    }
    const QString exe = Rx::wsprdExecutable(m_wsprdDir);
    const QStringList args = Rx::wsprdArguments(m_dataDir, job.dial, m_opt.deep, m_opt.quick, m_opt.osd, job.wav);
    if (!m_runner->start(exe, args, m_dataDir)) {
        m_error = m_runner->errorString();
        emit problem(m_error);
        return;
    }
    m_running = job;
    Rx::debugLog("Record_Decode END (decode running in background): " + QFileInfo(exe).fileName() + ' '
                 + args.join(' '));
    emit statusChanged("decoding");
}

void RxCycle::onDecoded(bool ok, const QString &output, const QString &error)
{
    if (!m_running)
        return;
    const Job job = *m_running;
    m_running.reset();
    if (!error.isEmpty())
        Rx::debugLog("decoder: " + error);
    Rx::DecodeResult r = Rx::parseWsprdOutput(output, job.cycle, job.dial.toDouble(), m_call, m_loc);
    if (!ok || !r.finished) {
        m_error = error.isEmpty() ? "wsprd gave no result" : error;
        emit problem("Decoder: " + m_error);
    } else {
        emit decoded(r, job.dial, job.cycle);
    }
    emit statusChanged(isRecording() ? "receiving" : "idle");
    if (m_queued) {
        const Job next = *m_queued;
        m_queued.reset();
        runDecode(next);
    }
}
