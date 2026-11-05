// Transmit: slot look-up, antenna / filter / frequency change before each cycle, PTT and WSPR audio.
// C#: daytimer_Action (odd :54-57 findSlot, odd :45 blockDecodes), WSPRtimer_Action, StartTX,
// WSPR_Transmit, PTT, wsprTXtimer_Action, stopTX, flashTX, TXrunbutton / TXenablebutton /
// enableTXcheckBox / stopTestbutton / stopWAVbutton, RXblocktimer.
//
// Differences from the C# version:
//  - "Dry run" (on until you untick it): everything happens - slot look-up, antenna switch, filter,
//    rig frequency, WSPR audio to the TX sound card - except PTT. With VOX ticked the audio is not
//    played either, since VOX would key the rig. Receive carries on during a dry-run cycle;
//  - a failed antenna switch, rig frequency change or PTT stops that transmission
//    (C# carried on, transmitting on the previous antenna / frequency);
//  - the audio starts at even :01 from a timer (C# busy-waited the UI thread until second 1);
//  - TX ends when the audio has played out (110.6 s), with a 114 s backstop, instead of a fixed 112 s timer;
//  - unticking "Enable TX" during a transmission stops it;
//  - status goes to the status bar and the debug log instead of 1-second pop-ups.
#include "mainwindow.h"
#include "rigctld.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "rigctl.h"
#include "rxform.h"
#include "rxspot.h"
#include "txlog.h"
#include "txplan.h"
#include "wspraudioout.h"
#include "wsprmessage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <cmath>

namespace {
constexpr int kBackstopMs = 114000;   // PTT off by odd :54 whatever happens to the audio
constexpr int kHardwareTimeoutMs = 1500;
}

bool MainWindow::isDryRun() const
{
    return !m_dryRunBox || m_dryRunBox->isChecked();
}

bool MainWindow::txSlotActiveNow() const
{
    return m_slotActive && ui->enableTXcheckBox->isChecked() && !isDryRun();
}

void MainWindow::setupTransmit()
{
    if (!m_txAudio) {
        m_txAudio = new WsprAudioOut(this);
        connect(m_txAudio, &WsprAudioOut::finished, this, [this] {
            if (m_transmitting)
                QTimer::singleShot(200, this, [this] { if (m_transmitting) stopTransmit("TX done", true); });
        });
    }
    if (!m_txAudioTimer) {
        m_txAudioTimer = new QTimer(this);
        m_txAudioTimer->setSingleShot(true);
        connect(m_txAudioTimer, &QTimer::timeout, this, &MainWindow::startTxAudio);
    }
    wsprTXtimer->setSingleShot(true);    // backstop, started with each transmission
    RXblocktimer->setSingleShot(true);

    if (!m_dryRunBox) {
        m_dryRunBox = new QCheckBox("Dry run (no PTT)", ui->tabPage2);
        m_dryRunBox->setObjectName("dryRuncheckBox");
        m_dryRunBox->setToolTip("Switch antenna, filter and frequency and play the WSPR audio, but don't key the rig.\n"
                                "With VOX ticked the audio isn't played either.");
        const QRect g = ui->enableTXcheckBox->geometry();
        m_dryRunBox->setGeometry(g.x(), g.bottom() + 2, 140, 18);
        m_dryRunBox->setChecked(QSettings().value("tx/dryRun", true).toBool());
        m_dryRunBox->show();
        connect(m_dryRunBox, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue("tx/dryRun", on);
            enableTXcheckBox_CheckedChanged(ui->enableTXcheckBox->isChecked());
            if (!on && m_transmitting)
                txStatus("Dry run off: PTT will be keyed from the next slot");
        });
    }
    enableTXcheckBox_CheckedChanged(ui->enableTXcheckBox->isChecked());
    {
        const QSignalBlocker b(ui->noSkedcheckBox);   // label matches the box at start-up
        ui->skedstoplabel->setText(ui->noSkedcheckBox->isChecked() ? "Sked disabled" : "Sked enabled");
        ui->skedstoplabel->adjustSize();
    }
    // picking a default antenna switches to it straight away (C#: DefaultAntcomboBox_SelectedIndexChanged);
    // "activated" = chosen by the user, not the list being refilled
    connect(ui->DefaultAntcomboBox, &QComboBox::activated, this,
            [this](int i) { switchToAntenna(ui->DefaultAntcomboBox->itemText(i)); });
}

void MainWindow::DefaultAntcomboBox_SelectedIndexChanged(int index)
{
    if (index >= 0)
        m_cfg.defaultAnt = ui->DefaultAntcomboBox->itemText(index);   // C#: defaultAnt = DefaultAntcomboBox.Text
}

void MainWindow::switchToAntenna(const QString &name)
{
    if (name.isEmpty())
        return;
    if (m_transmitting) {   // C#: selectAntenna - not while transmitting
        txStatus("Transmitting: antenna not changed (it will be used from the next test)");
        return;
    }
    const auto ant = TxPlan::antennaFor(m_antennas, name, name);
    if (!ant)
        return;
    const auto r = TxPlan::selectAntenna(*ant, m_sw, kHardwareTimeoutMs);
    if (!r.ok) {
        txStatus("Antenna switch error: " + r.message, true);
        return;
    }
    m_txAntenna = ant->name;
    ui->TXRXAntlabel->setText(ant->name);
    ui->TXRXAntlabel2->setText(ant->name);
    txStatus("Antenna: " + r.message);
}

void MainWindow::tuneTo(double mhz)
{
    if (mhz <= 0.0)
        return;
    if (m_transmitting) {
        txStatus("Transmitting: frequency not changed (it will be used from the next test)");
        return;
    }
    if (m_noRigctld) {
        selectManualFrequency(mhz);   // C#: testFreq - "change frequency manually"
        return;
    }
    const qint64 hz = qint64(std::llround(mhz * 1e6));
    const qint64 dial = correctedDialHz(hz, m_rigCfg.ppm);
    if (!txRig()->setFrequency(dial)) {
        txStatus("Rig frequency not changed: " + m_rig->lastError(), true);
        return;
    }
    m_dialMHz = QString::number(mhz, 'f', 6);
    if (m_rx)
        m_rx->setFrequency(m_dialMHz);
    showTxState(QDateTime::currentDateTimeUtc());
    txStatus(QString("Rig tuned to %1 MHz%2").arg(mhz, 0, 'f', 6)
                 .arg(TxPlan::isReceiveOnly(mhz) ? " (receive only on this band)" : ""));
}

void MainWindow::txStatus(const QString &text, bool error)
{
    statusBar()->showMessage(QDateTime::currentDateTimeUtc().toString("HH:mm:ss ") + text, error ? 0 : 60000);
    Rx::debugLog("TX: " + text);
    if (error)
        Msg::timed(this, text, "Transmit", 4000);
}

double MainWindow::txAlpha() const
{
    bool ok = false;
    const double a = ui->AlphacomboBox->currentText().trimmed().toDouble(&ok);
    return ok && a > 0.0 && a <= 1.0 ? a : 0.1;   // C#: defaultAlpha, 0.1 if unset
}

// Called every second from daytimer_Tick
void MainWindow::txTick(const QDateTime &utc)
{
    const QTime t = utc.time();

    if (t.minute() % 2 == 0 && t.second() >= 3 && t.second() <= 10) {
        m_lookedUp = false;   // C#: Flag = false
        m_fired = false;
    }

    // C#: odd :45 - stop decoding in case the transmission ends before the last decode starts
    if (TxPlan::isBlockMoment(t) && m_transmitting && !isDryRun() && m_rx) {
        m_rx->setBlockDecodes(true);
        RXblocktimer->start(30000);
    }

    if (TxPlan::isLookupWindow(t) && !m_lookedUp) {
        m_lookedUp = true;
        lookUpNextSlot(utc);
    }

    if (m_txSlot && !m_prepDone && TxPlan::isPrepareWindow(t) && TxPlan::nextCycleStart(utc) == m_txCycleUtc) {
        m_prepDone = true;
        prepareSlot();
    }

    if (m_txSlot && !m_fired && TxPlan::isFireWindow(t) && TxPlan::currentCycleStart(utc) == m_txCycleUtc) {
        m_fired = true;
        if (m_slotActive && ui->enableTXcheckBox->isChecked() && !m_transmitting) {
            startTransmit();
        } else {
            if (m_txTest == TxTest::Scheduled || m_txTest == TxTest::Remote)
                m_txTest = TxTest::None;   // cancelled at the last moment (switch / rig error), or receive only
            if (m_rx)
                m_rx->setLabel("receiving");
        }
    }

    showTxState(utc);
}

// C#: findSlot(-1, date, nexttime) and the checks after it
void MainWindow::lookUpNextSlot(const QDateTime &utc)
{
    if (m_transmitting)
        return;   // still finishing the last one (cannot happen with a 114 s backstop, but be safe)
    if ((m_txTest == TxTest::Scheduled || m_txTest == TxTest::Remote) && m_txSlot
        && m_txCycleUtc >= TxPlan::nextCycleStart(utc))
        return;   // "Wspr TX Test" or remote "Go next slot" queued for this cycle
    m_txSlot.reset();
    m_slotActive = false;
    m_prepDone = false;
    m_fired = false;
    if (ui->noSkedcheckBox->isChecked())
        return;   // C#: "Schedule not enabled!"

    const QDateTime cycle = TxPlan::nextCycleStart(utc);
    const QDateTime key = TxPlan::slotKey(cycle, ui->LTcheckBox->isChecked());
    const auto slot = m_store.find(key.date(), QTime(key.time().hour(), key.time().minute()));
    if (!slot) {
        if (!m_store.lastError().isEmpty())
            Rx::debugLog("TX: slot look-up: " + m_store.lastError());
        return;
    }
    m_txSlot = slot;
    m_txCycleUtc = cycle;
    if (manualFrequencyMode()) {
        // C#: changeFreq with neither rigctld nor a rig: the frequency picked by hand, not the slot's
        if (m_manualMHz <= 0.0)
            Msg::timed(this, "Don't forget to select the TX/RX frequency", "", 2000);
        m_txSlot->freq = m_manualMHz > 0.0 ? m_manualMHz : m_cfg.defaultF;
    }
    m_slotActive = slot->active;
    ui->mtypelabel->setText(QString::number(slot->msgType));
    ui->slotnolabel->setText(QString::number(slot->slotNo));

    if (m_slotActive && !ui->enableTXcheckBox->isChecked()) {
        txStatus(QString("Slot %1: TX not enabled - receiving on %2 MHz")
                     .arg(key.toString("HH:mm")).arg(slot->freq, 0, 'f', 4));
        Msg::timed(this, "Warning: TX not enabled", "TX Status", 4000);
        m_slotActive = false;
    }
    if (!m_noRigctld && !txRig()->isReachable()) {   // (a Non Rigctl TX rig is checked when it is sent the TX command)
        // C#: "Error: RigCtld not running" - WSPRtimer not started, so no change and no TX
        txStatus("rigctld not running: slot " + key.toString("HH:mm") + " skipped", true);
        m_txSlot.reset();
        m_slotActive = false;
        return;
    }
    txStatus(QString("Next slot %1: %2 MHz, %3, %4 dBm%5")
                 .arg(key.toString("HH:mm"))
                 .arg(slot->freq, 0, 'f', 4)
                 .arg(slot->antenna)
                 .arg(slot->powerdBm)
                 .arg(m_slotActive ? (isDryRun() ? ", TX (dry run)" : ", TX") : ", RX only"));
}

// C#: WSPRtimer_Action when down <= 5 - activateAntSwitch, changeFilter, activateTX, rxForm.set_frequency
void MainWindow::prepareSlot()
{
    if (!m_txSlot)
        return;
    const SlotRecord &s = *m_txSlot;
    const qint64 hz = qint64(std::llround(s.freq * 1e6));
    QStringList done;
    bool ok = true;

    const auto ant = TxPlan::antennaFor(m_antennas, s.antenna, ui->DefaultAntcomboBox->currentText());
    if (ant) {
        const auto r = TxPlan::selectAntenna(*ant, m_sw, kHardwareTimeoutMs);
        if (r.ok) {
            m_txAntenna = ant->name;
            ui->TXRXAntlabel->setText(ant->name);
            ui->TXRXAntlabel2->setText(ant->name);
            done << r.message;
        } else {
            ok = false;
            txStatus("Antenna switch error: " + r.message, true);
        }
    } else {
        m_txAntenna = s.antenna;
        done << "antenna \"" + s.antenna + "\" not in the antenna list";
    }

    const auto f = TxPlan::selectFilter(m_fil, m_selectedFilter, hz, kHardwareTimeoutMs);
    if (!f.ok) {
        ok = false;
        txStatus("Filter error: " + f.message, true);
    } else if (!f.message.isEmpty()) {
        done << f.message;
    }

    if (otherRigInUse()) {
        // C#: changeFreq with selectedRig > 0 - the rig's TX command, with freq/call/grid/pwr/offset filled in
        const OtherRig *r = otherRig(m_selectedRig);
        QString err;
        if (r->txCommand.isEmpty()) {
            done << r->name + " (no TX command)";
        } else if (sendOtherRig(otherRigCommand(r->txCommand, s), &err, true)) {
            done << r->name + " set for " + QString::number(s.freq, 'f', 4) + " MHz";
        } else {
            ok = false;
            txStatus("Error sending TX command to " + r->name + ": " + err, true);
        }
    } else if (m_noRigctld) {
        done << "rig not controlled (rigctld off)";
    } else {
        const qint64 dial = correctedDialHz(hz, m_rigCfg.ppm);
        if (txRig()->setFrequency(dial)) {
            done << (dial == hz ? QString("rig %1 MHz").arg(s.freq, 0, 'f', 6)
                                : QString("rig %1 MHz (%2%3 ppm)").arg(dial / 1e6, 0, 'f', 6)
                                      .arg(m_rigCfg.ppm > 0 ? "+" : "").arg(m_rigCfg.ppm, 0, 'f', 2));
        } else {
            ok = false;
            txStatus("Rig frequency not changed: " + m_rig->lastError(), true);
        }
    }
    // the receiver labels the next cycle with this frequency (the last cycle was recorded with the old one)
    m_txMHz = s.freq;
    m_dialMHz = QString::number(s.freq, 'f', 6);
    if (m_rx)
        m_rx->setFrequency(m_dialMHz);

    if (!ok && m_slotActive) {
        m_slotActive = false;   // don't transmit on the wrong antenna / frequency
        txStatus("Transmission cancelled for this slot", true);
    }
    txStatus("Ready: " + done.join(", "));
}

// C#: StartTX / WSPR_Transmit up to the audio
void MainWindow::startTransmit()
{
    if (!m_txSlot)
        return;
    const SlotRecord &s = *m_txSlot;

    // C#: msgType from the slot, with the override box (findSlot) and Type2 box (WSPR_Transmit)
    int msgType = s.msgType;
    if (ui->overcheckBox->isChecked())
        msgType = ui->Type2checkBox->isChecked() ? 3 : 1;
    if (!ui->Type2checkBox->isChecked())
        msgType = 1;
    ui->overlabel->setText(ui->overcheckBox->isChecked() ? "msg type override on" : "");

    TxMessageRequest req;
    req.callsign = ui->CalltextBox->text().trimmed().toUpper();
    req.baseCall = ui->baseCalltextBox->text().trimmed().toUpper();
    if (req.baseCall.isEmpty())
        req.baseCall = req.callsign;
    req.locator = ui->LocatortextBox->text().trimmed();
    req.powerdBm = s.powerdBm;
    req.msgType = msgType;
    req.slotNo = s.slotNo;
    req.oneMsg = ui->asOnecheckBox->isChecked();
    req.allowType2 = ui->Type2checkBox->isChecked();
    req.longLocator = ui->longcheckBox->isChecked();
    const TxMessage msg = planMessage(req);
    if (!msg.ok()) {
        txStatus("Error encoding message: " + msg.error, true);
        m_slotActive = false;
        return;
    }
    ui->msgTypelabel->setText(QString("Message type %1").arg(msg.wsprType));
    ui->msgTypelabel2->setText(ui->msgTypelabel->text());
    m_txCall = msg.wsprType == 2 ? req.callsign : req.baseCall;

    const bool dry = isDryRun();
    const bool vox = ui->VoxcheckBox->isChecked();
    // C#: no PTT with "wider tones" ticked - 100 Hz spacing is for listening, not for the air (it is
    // 400 Hz wide, outside the 200 Hz WSPR window). With VOX the audio would key the rig, so refuse.
    const bool wide = ui->testTonescheckBox->isChecked();
    if (wide && vox && m_txTest != TxTest::AudioOnly) {
        txStatus("Wider test tones with VOX would key the rig - not sent", true);
        m_slotActive = false;
        return;
    }
    if (!dry && !vox && !wide && m_txTest != TxTest::AudioOnly) {
        QString err;
        if (!keyPtt(true, &err)) {
            txStatus("Unable to key TX: " + err, true);
            m_slotActive = false;
            return;
        }
    }
    m_transmitting = true;
    if (m_rx && !dry && m_txTest != TxTest::AudioOnly)
        m_rx->setLabel("idle/not receiving");

    // keep the message for the audio start
    m_txAudio->setProperty("symbols", QByteArray(reinterpret_cast<const char *>(msg.symbols->data()),
                                                 int(msg.symbols->size())));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 toStart = now.msecsTo(m_txCycleUtc.addSecs(1));   // C#: start at even minute + 1 s
    if (dry && vox)
        txStatus("Dry run with VOX: audio not played (it would key the rig)");
    else
        m_txAudioTimer->start(int(qBound(qint64(0), toStart, qint64(3000))));
    wsprTXtimer->start(int(qBound(qint64(1000), now.msecsTo(m_txCycleUtc.addMSecs(kBackstopMs)), qint64(kBackstopMs))));

    if (m_txTest == TxTest::Now || m_txTest == TxTest::AudioOnly)
        testPTTtimer->start(30000);   // C#: testPTTtimer - test transmissions last 30 s
    m_txWide = wide;
    const QString what = wide ? QString("WIDE TEST TONES (no PTT)")
                         : m_txTest == TxTest::AudioOnly ? QString("AUDIO TEST")
                         : dry                         ? QString("DRY RUN")
                         : m_txTest == TxTest::Remote  ? QString("TX (remote)")
                         : m_txTest != TxTest::None    ? QString("TX TEST")
                                                       : QString("TX");
    txStatus(QString("%1 %2 on %3 MHz, %4, %5 dBm%6")
                 .arg(what, msg.text)
                 .arg(s.freq, 0, 'f', 4)
                 .arg(m_txAntenna)
                 .arg(s.powerdBm)
                 .arg(vox && !dry ? " (VOX)" : ""));
}

void MainWindow::startTxAudio()
{
    if (!m_transmitting || !m_txSlot)
        return;
    const QByteArray b = m_txAudio->property("symbols").toByteArray();
    if (b.size() != WsprModulator::kSymbols) {
        stopTransmit("no message to send", false);
        return;
    }
    std::array<uint8_t, WsprModulator::kSymbols> sym{};
    std::copy(b.begin(), b.end(), sym.begin());

    WsprModulator::Params p;
    p.offsetHz = m_txSlot->offset;
    p.testTones = ui->testTonescheckBox->isChecked();
    p.alpha = txAlpha();
    m_txVolume = float(txLevelFor(m_txSlot->freq));   // C#: Volume = findFreqVol(F)
    m_txAudio->setDevice(m_audioOutName);
    if (!m_txAudio->start(sym, p, m_txVolume)) {
        txStatus("TX audio: " + m_txAudio->lastError(), true);
        stopTransmit("audio error", false);
    }
}

// C#: wsprTXtimer_Action (completed) / stopTX (stopped)
void MainWindow::stopTransmit(const QString &why, bool completed)
{
    const bool was = m_transmitting;
    m_txAudioTimer->stop();
    wsprTXtimer->stop();
    testPTTtimer->stop();
    if (m_txAudio && m_txAudio->isPlaying())
        m_txAudio->stop();
    if (m_pttOn) {
        QString err;
        if (!keyPtt(false, &err)) {
            // try once more: leaving the rig keyed is the worst outcome
            QTimer::singleShot(500, this, [this] {
                QString e2;
                if (!keyPtt(false, &e2))
                    txStatus("PTT OFF FAILED - check the rig: " + e2, true);
            });
        }
    }
    m_transmitting = false;
    m_slotActive = false;
    ui->testTonescheckBox->setChecked(false);   // C#: test tones are for one transmission
    ui->mtypelabel->setText("-");
    ui->slotnolabel->setText("-");
    if (!was)
        return;

    const bool dry = isDryRun();
    const TxTest test = m_txTest;
    m_txTest = TxTest::None;
    const std::optional<SlotRecord> sent = m_txSlot;
    m_txSlot.reset();   // this slot is over (the next one is looked up at odd :54)
    if (completed && !dry && !m_txWide && sent && (test == TxTest::None || test == TxTest::Scheduled || test == TxTest::Remote))   // C#: WSPR_Log
        TxLog::appendTransmission(m_txCycleUtc, m_txCall, sent->freq, ui->LocatortextBox->text().trimmed(),
                                  sent->powerdBm, m_txAntenna);
    txStatus((dry ? "Dry run ended: " : "TX ended: ") + why);
}

RigCtl *MainWindow::txRig()
{
    if (!m_rig)
        m_rig = std::make_unique<RigCtl>();
    m_rig->setAddress(m_rigCfg.ipv4.isEmpty() ? QString("127.0.0.1") : m_rigCfg.ipv4,
                      m_rigCfg.port.toInt() > 0 ? m_rigCfg.port.toInt() : 4532);
    m_rig->setTimeout(1500);
    return m_rig.get();
}

// C#: PTT. Only through rigctld here (the Non Rigctl TX tab is not ported yet).
bool MainWindow::keyPtt(bool on, QString *error)
{
    if (otherRigInUse()) {   // C#: PTT with selectedRig > 0 - TXptt to start, RXcommand to stop
        const OtherRig *r = otherRig(m_selectedRig);
        QString cmd = on ? r->txPtt : r->rxCommand;
        if (m_txSlot)
            cmd = otherRigCommand(cmd, *m_txSlot);
        const bool ok = sendOtherRig(cmd, error);
        if (ok)
            m_pttOn = on;
        return ok;
    }
    if (m_noRigctld) {
        *error = "rigctld is switched off (Rigctl rigs tab) - tick VOX or use rigctld";
        return false;
    }
    const bool ok = txRig()->setPtt(on);
    if (ok)
        m_pttOn = on;
    else
        *error = m_rig->lastError();
    return ok;
}

// C#: countdown part of WSPRtimer_Action and flashTX
namespace {
// C# colours (Form1.Designer / flashTX): white bold text on Olive (receiving, no slot), RoyalBlue (in a receive
// slot), OrangeRed / Orange flashing (transmitting)
const QString kReceiving = "background-color: #808000; color: white; font-weight: bold;";   // Color.Olive
const QString kRxSlot = "background-color: #4169E1; color: white; font-weight: bold;";      // Color.RoyalBlue
const QString kTxOn = "background-color: #FF4500; color: white; font-weight: bold;";        // Color.OrangeRed
const QString kTxOff = "background-color: #FFA500; color: white; font-weight: bold;";       // Color.Orange

// "14.0956", "50.293", "7.03861": at least 4 decimals, more only when the dial has them
QString mhzText(double mhz)
{
    QString t = QString::number(mhz, 'f', 6);
    while (t.endsWith('0') && t.indexOf('.') >= 0 && t.size() - t.indexOf('.') > 5)
        t.chop(1);
    return t + " MHz";
}
} // namespace

// C#: WSPRtimer "TX in N seconds" / "RX in: N seconds" before a slot, then flashTX "TX time: n" / "RX time: n"
// counting 0..110 through it. Unlike C# (which counted down only from :54, once the slot was read), the next
// slot is read early for the display, so the countdown runs from the end of the last slot.
QString MainWindow::countdownText(const QDateTime &utc)
{
    const bool enabled = ui->enableTXcheckBox->isChecked();
    if (m_transmitting)
        return QString("TX time: %1").arg(qBound(qint64(0), m_txCycleUtc.secsTo(utc), qint64(110)));
    if (m_txSlot && utc >= m_txCycleUtc && m_txCycleUtc.secsTo(utc) <= 110)
        return QString("%1 time: %2").arg(m_slotActive && enabled ? "TX" : "RX").arg(m_txCycleUtc.secsTo(utc));

    // next slot: the one already looked up (or a queued test), else an early look at the schedule
    const QDateTime next = TxPlan::nextCycleStart(utc);
    if (m_previewCycle != next) {
        m_previewCycle = next;
        m_preview.reset();
        if (!ui->noSkedcheckBox->isChecked()) {
            const QDateTime key = TxPlan::slotKey(next, ui->LTcheckBox->isChecked());
            m_preview = m_store.find(key.date(), QTime(key.time().hour(), key.time().minute()));
        }
    }
    std::optional<bool> tx;
    if (m_txSlot && utc < m_txCycleUtc)
        tx = m_slotActive && enabled;
    else if (m_preview)
        tx = m_preview->active && enabled;
    if (!tx)
        return QString();   // no slot next
    const qint64 down = utc.secsTo(next);
    return *tx ? QString("TX in %1 seconds").arg(down) : QString("RX in: %1 seconds").arg(down);
}

void MainWindow::showTxState(const QDateTime &utc)
{
    const int down = TxPlan::secondsToNextEven(utc.time());
    QString text, style;
    const QString slotF = m_txSlot ? " " + mhzText(m_txSlot->freq) : QString();
    if (m_transmitting) {
        m_flash = !m_flash;
        text = (m_txTest == TxTest::AudioOnly ? "Audio test" : isDryRun() ? "DRY RUN"
                : (m_txTest != TxTest::None && m_txTest != TxTest::Remote) ? "TX test" : "TX")
               + slotF;
        style = m_flash ? kTxOn : kTxOff;
    } else if (m_txSlot && !(m_slotActive && ui->enableTXcheckBox->isChecked())
               && utc >= m_txCycleUtc && utc < m_txCycleUtc.addSecs(120)) {
        // in a receive slot (inactive slot, or TX not enabled): blue for the whole 2 minutes
        text = "RX" + slotF;
        style = kRxSlot;
    } else if (m_txSlot && utc < m_txCycleUtc) {
        // the last seconds before a slot: still receiving on the old frequency
        const bool tx = m_slotActive && ui->enableTXcheckBox->isChecked();
        text = (tx ? (isDryRun() ? "Dry run next" : m_txTest == TxTest::Scheduled ? "TX test next" : m_txTest == TxTest::Remote ? "Remote TX next" : "TX next") : "RX next")
               + slotF;
        style = tx ? kReceiving : kRxSlot;
    } else {
        const double dial = m_dialMHz.toDouble();
        if (dial <= 0.0)
            text = "TX/RX idle";
        else
            text = QString(m_stopRX ? "Idle " : "RX ") + mhzText(dial);   // what the rig is on now
        style = kReceiving;
    }
    ui->countdownlabel->setText(countdownText(utc));
    ui->countdownlabel2->setText(ui->countdownlabel->text());
    ui->countdownlabel->adjustSize();
    ui->countdownlabel2->adjustSize();
    for (QPushButton *b : {ui->TXrunbutton, ui->TXrunbutton2}) {
        b->setText(text);
        b->setStyleSheet(style);
    }
}

// ---- buttons ----------------------------------------------------------------------------------

// C#: TXrunbutton_Click - stop a transmission (C# asked "Stop TX (y/n)?" first); also cancels a queued
// Wspr TX Test. Either TX/RX button (main page or TX config page) does it.
void MainWindow::TXrunbutton_Click()
{
    if (m_transmitting) {
        stopTransmit("stopped with the TX button", false);
    } else if (m_txTest == TxTest::Scheduled || m_txTest == TxTest::Remote) {
        txStatus(m_txTest == TxTest::Remote ? "Remote next slot cancelled" : "Wspr TX Test cancelled");
        m_txTest = TxTest::None;
        m_txSlot.reset();
        m_slotActive = false;
    }
    showTxState(QDateTime::currentDateTimeUtc());
}

void MainWindow::TXenablebutton_Click()
{
    ui->enableTXcheckBox->setChecked(!ui->enableTXcheckBox->isChecked());
}

void MainWindow::enableTXcheckBox_CheckedChanged(bool checked)
{
    const QString style = checked ? "background-color: red; color: white;" : "background-color: green; color: white;";
    ui->enableTXcheckBox->setStyleSheet(style);
    ui->TXenablebutton->setStyleSheet(style);
    ui->TXenablebutton->setText(checked ? (isDryRun() ? "TX on (dry run)" : "TX Enabled") : "Enable TX");
    if (!checked && m_transmitting)
        stopTransmit("TX disabled", false);
}

void MainWindow::stopTestbutton_Click()
{
    stopTransmit("stopped", false);
}

void MainWindow::stopWAVbutton_Click()
{
    if (m_txAudio)
        m_txAudio->stop();   // C#: stopPlay = true (PTT stays as it is until the end of the slot)
}

void MainWindow::wsprTXtimer_Tick()
{
    if (m_transmitting)
        stopTransmit("end of slot", true);
}

void MainWindow::RXblocktimer_Tick()
{
    if (m_rx)
        m_rx->setBlockDecodes(false);
}

// ---- Test TX buttons (Config tab) ----------------------------------------------------------------

// Frequency from the test box (filled by clicking the frequency list), default antenna, offset and power
std::optional<SlotRecord> MainWindow::testSlotFromUi(QString *error) const
{
    bool ok = false;
    QString f = ui->testFtextBox->text().trimmed();
    if (f.isEmpty() && ui->FlistBox->currentItem())
        f = ui->FlistBox->currentItem()->text().trimmed();
    const double mhz = f.toDouble(&ok);
    if (!ok || mhz <= 0.0) {
        *error = "Select frequency from list first";
        return std::nullopt;
    }
    if (TxPlan::isReceiveOnly(mhz)) {
        *error = "RX only on 22 or 8m";
        return std::nullopt;
    }
    if (ui->CalltextBox->text().trimmed().isEmpty() || ui->defaultOfftextBox->text().trimmed().isEmpty()) {
        *error = "Blank callsign or offset";
        return std::nullopt;
    }
    SlotRecord s;
    s.freq = mhz;
    s.offset = qBound(0, ui->defaultOfftextBox->text().trimmed().toInt(), 200);
    s.powerdBm = ui->defaultpwrcomboBox->currentText().trimmed().toInt();
    s.antenna = ui->DefaultAntcomboBox->currentText();
    s.active = true;
    s.slotNo = 1;
    s.msgType = ui->Type2checkBox->isChecked() ? 3 : 1;   // C#: TXTestbutton
    return s;
}

bool MainWindow::txTestAllowed(bool needRig)
{
    if (m_transmitting) {
        Msg::timed(this, "Already transmitting - Stop Transmit first", "Test TX", 3000);
        return false;
    }
    if (!ui->enableTXcheckBox->isChecked()) {
        Msg::timed(this, "TX not enabled", "Test TX", 3000);
        return false;
    }
    if (needRig && !m_noRigctld && !txRig()->isReachable()) {
        Msg::timed(this, "Error: RigCtld not running", "Test TX", 3000);
        return false;
    }
    if (needRig && m_noRigctld && !otherRigInUse() && !ui->VoxcheckBox->isChecked() && !isDryRun()) {
        Msg::timed(this, "rigctld is off: tick VOX to test without it", "Test TX", 3000);
        return false;
    }
    return true;
}

// C#: TXTestbutton ("Wspr TX Test") - a full WSPR transmission in the next 2-minute slot
void MainWindow::TXTestbutton_Click()
{
    QString err;
    const auto s = testSlotFromUi(&err);
    if (!s) {
        Msg::timed(this, err, "Wspr TX Test", 3000);
        return;
    }
    if (!txTestAllowed(true))
        return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime cycle = TxPlan::nextCycleStart(now);
    if (now.secsTo(cycle) < 4)
        cycle = cycle.addSecs(120);   // too late to switch and tune for this one
    m_txSlot = s;
    m_txCycleUtc = cycle;
    m_slotActive = true;
    m_prepDone = false;
    m_fired = false;
    m_txTest = TxTest::Scheduled;
    ui->mtypelabel->setText(QString::number(s->msgType));
    ui->slotnolabel->setText("1");
    txStatus(QString("Wspr TX Test queued for %1 UTC: %2 MHz, %3, %4 dBm%5")
                 .arg(cycle.toString("HH:mm"))
                 .arg(s->freq, 0, 'f', 4)
                 .arg(s->antenna)
                 .arg(s->powerdBm)
                 .arg(isDryRun() ? " (dry run)" : ""));
}

// C#: TestTXbutton ("Test TX/PTT") - switch, tune and transmit straight away for 30 s
void MainWindow::TestTXbutton_Click()
{
    QString err;
    auto s = testSlotFromUi(&err);
    if (!s) {
        Msg::timed(this, err, "Test TX/PTT", 3000);
        return;
    }
    s->msgType = 1;   // C#: flatcode - slot 1, type 1, default offset
    if (!txTestAllowed(true))
        return;
    m_txSlot = s;
    m_slotActive = true;
    m_txTest = TxTest::Now;
    prepareSlot();
    if (!m_slotActive) {   // switch / filter / rig error
        m_txTest = TxTest::None;
        m_txSlot.reset();
        return;
    }
    m_txCycleUtc = QDateTime::currentDateTimeUtc().addSecs(-1);   // audio now, not at the next even minute
    m_fired = true;                                                // the clock tick must not start it again
    startTransmit();
    if (!m_transmitting) {
        m_txTest = TxTest::None;
        m_txSlot.reset();
    }
}

// C#: testButton ("Test wspr", StartTX(true)) - 30 s of WSPR audio without PTT, to check the level
void MainWindow::testButton_Click()
{
    QString err;
    auto s = testSlotFromUi(&err);
    if (!s) {
        Msg::timed(this, err, "Test wspr", 3000);
        return;
    }
    if (m_transmitting) {
        Msg::timed(this, "Already transmitting - Stop Transmit first", "Test wspr", 3000);
        return;
    }
    if (ui->VoxcheckBox->isChecked()) {
        Msg::timed(this, "VOX is ticked: the audio would key the rig. Use Test TX/PTT instead.", "Test wspr", 4000);
        return;
    }
    s->msgType = 1;
    m_txSlot = s;
    m_txAntenna = s->antenna;
    m_slotActive = true;
    m_txTest = TxTest::AudioOnly;
    m_txCycleUtc = QDateTime::currentDateTimeUtc().addSecs(-1);
    startTransmit();
    if (!m_transmitting) {
        m_txTest = TxTest::None;
        m_txSlot.reset();
    }
}

void MainWindow::testPTTtimer_Tick()
{
    if (m_transmitting)
        stopTransmit("30 s test ended", false);   // C#: testPTTtimer - PTT off
}

// C#: FlistBox_SelectedIndexChanged - the clicked frequency is the test frequency
void MainWindow::FlistBox_SelectedIndexChanged(int row)
{
    if (row < 0)
        return;
    if (row < ui->FlistBox2->count())
        ui->FlistBox2->setCurrentRow(row);
    ui->testFtextBox->setText(ui->FlistBox->item(row)->text().trimmed());
    tuneTo(ui->testFtextBox->text().toDouble());   // C#: testFreq(false)
}

void MainWindow::testFtextBox_TextChanged(const QString &) {}

// C#: noSkedcheckBox_CheckedChanged - "Sked disabled" / "Sked enabled" on the main page
void MainWindow::noSkedcheckBox_CheckedChanged(bool checked)
{
    ui->skedstoplabel->setText(checked ? "Sked disabled" : "Sked enabled");
    ui->skedstoplabel->setStyleSheet(checked ? "color: red; font-weight: bold;" : QString());
    ui->skedstoplabel->adjustSize();
    m_previewCycle = QDateTime();   // re-read (or clear) the next slot for the countdown
    m_preview.reset();
    if (checked && !m_transmitting && m_txTest == TxTest::None) {
        m_txSlot.reset();          // a slot already read for the next cycle is not used either
        m_slotActive = false;
    }
    showTxState(QDateTime::currentDateTimeUtc());
    txStatus(checked ? "Schedule disabled" : "Schedule enabled");
}

// C#: skedstoplabel_DoubleClick - double-click the label to switch the schedule on or off
void MainWindow::skedstoplabel_DoubleClick()
{
    ui->noSkedcheckBox->setChecked(!ui->noSkedcheckBox->isChecked());
}

// rigctld off and no Non Rigctl TX rig: the rig is tuned by hand, so the receiver (and the wsprnet
// uploads) must be told which frequency that is - picked in the list on the Slots or TX Configuration
// tab. Slot frequencies are then ignored (C#: "slot freq will be ignored").
void MainWindow::selectManualFrequency(double mhz)
{
    if (mhz <= 0.0)
        return;
    m_manualMHz = mhz;
    m_dialMHz = QString::number(mhz, 'f', 6);
    if (m_rx)
        m_rx->setFrequency(m_dialMHz);
    const QString f = QString::number(mhz, 'f', 4);
    for (QListWidget *l : {ui->FlistBox, ui->FlistBox2}) {   // C#: the two lists follow each other
        const QSignalBlocker b(l);
        for (int i = 0; i < l->count(); ++i)
            if (std::abs(l->item(i)->text().toDouble() - mhz) < 1e-6)
                l->setCurrentRow(i);
    }
    showTxState(QDateTime::currentDateTimeUtc());
    txStatus(QString("Frequency selected: %1 MHz - set the rig to it by hand%2")
                 .arg(f, TxPlan::isReceiveOnly(mhz) ? " (receive only on this band)" : ""));
}

bool MainWindow::manualFrequencyMode() const
{
    return m_noRigctld && !otherRigInUse();
}

void MainWindow::WSPRtimer_Tick() {}   // replaced by txTick (driven by daytimer)
void MainWindow::daytimer2_Tick() {}   // C# ran the clock from daytimer2 while transmitting; daytimer keeps running here
