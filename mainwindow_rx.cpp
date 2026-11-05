// Form1's side of the receive window: set-up at load (C#: Read_Audio + rxForm.set_header),
// the clock / start-of-cycle calls from daytimer_Action, and the "stop RX" checkbox.
// and the RX & Sound tab (device lists, gain slider and level meter, wsprd folder, Save_Audio).
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "rxform.h"
#include "wspraudioin.h"
#include "wspraudioout.h"

#include <QDir>
#include <QFileInfo>
#include <QListWidget>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QRegularExpression>

#include "version.h"

QString MainWindow::rxCallsign() const
{
    // C#: C = baseCalltextBox.Text; if (Type2checkBox.Checked) C = CalltextBox.Text
    return (ui->Type2checkBox->isChecked() ? ui->CalltextBox->text() : ui->baseCalltextBox->text()).trimmed();
}

void MainWindow::setupReceiver()
{
    if (!m_rx)
        m_rx = new RxForm(nullptr);   // own top-level window, like the C# form
    const DbSettings db = m_store.settings();
    m_rx->store().setSettings(db);

    // C#: Read_Audio
    if (const auto rs = m_rx->store().readRxSettings()) {
        m_audioInName = rs->inputName;
        m_audioOutName = rs->outputName;
        m_inLevel = qBound(1, rs->inLevel, 500);
        m_outLevel = rs->outLevel;
        m_gain = m_inLevel / 100.0f;
        m_wsprdPath = rs->wsprdPath.trimmed();
        const QSignalBlocker b(ui->samecheckBox);
        ui->samecheckBox->setChecked(rs->sameDevice);
    }
    if (m_wsprdPath.isEmpty())
        m_wsprdPath = Rx::defaultDataDir();   // C#: root + "WSPR_Sked"
    ui->wsprdtextBox->setText(QDir::toNativeSeparators(m_wsprdPath));

    // C#: MonitorMic set the slider up: 1..500 = gain 0.01..5
    // Signals blocked for setRange too: it moves the slider from 0 to 1, and the handler then
    // overwrote the saved level with 1 (gain 0.01) before setValue below could apply it.
    {
        const QSignalBlocker b(ui->trackBarGain);
        ui->trackBarGain->setRange(1, 500);
        ui->trackBarGain->setTickInterval(50);
        ui->trackBarGain->setValue(m_inLevel);
    }
    ui->gainlabel->setText(QString::number(m_gain));
    ui->gainlabel->adjustSize();
    ui->volumeMeter1->setRange(0, 100);
    findSound();
    matchAudioList();

    m_rx->setGain(m_gain);
    m_rx->setHeader(rxCallsign(), db, ui->LocatortextBox->text(), m_audioInName, m_wsprdPath, WSPRSKED_VERSION);
    // dial frequency: from rigctld when it answers (getRigF), else the default frequency
    m_rx->setFrequency(m_dialMHz.isEmpty() ? QString::number(m_cfg.defaultF, 'f', 6) : m_dialMHz);
}

void MainWindow::showReceiver()
{
    if (m_rx && !m_stopRX)
        m_rx->show();
}

void MainWindow::receiverTick(const QDateTime &utc)
{
    if (!m_rx)
        return;
    // Always UTC: C# used local time here when "LT" was ticked, which shows local time as "UTC" and
    // would start cycles at the wrong moment in a time zone with an odd number of minutes' offset
    m_rx->setTime(utc.toString("HH:mm:ss"), QDateTime::currentDateTime().toString("HH:mm"));

    const int m = utc.time().minute(), s = utc.time().second();
    const bool startWindow = (m % 2 == 1 && s == 59) || (m % 2 == 0 && s <= 1);
    if (startWindow && !m_recordFlag) {
        m_recordFlag = true;
        if (txSlotActiveNow())
            m_rx->setLabel("idle/not receiving");
        else if (!m_stopRX)
            m_rx->startReceive();
    }
    if (m % 2 == 0 && s >= 7 && s <= 9)
        m_recordFlag = false;   // C#: allows for missed timer ticks
}

// ---- RX & Sound tab ---------------------------------------------------------------------------

namespace {
// "1: Microphone (USB Audio Device)" -> "Microphone (USB Audio Device)" (C# list text had the WinMM index)
QString bareName(QString n)
{
    n.remove(QRegularExpression("^\\d+:\\s*"));
    return n.trimmed();
}

// Find a saved name in a list: exact, then without the C# index, then a name Windows cut to 31 characters
int findName(const QListWidget *list, const QString &saved)
{
    const QString want = bareName(saved);
    if (want.isEmpty())
        return -1;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->text().compare(want, Qt::CaseInsensitive) == 0)
            return i;
    if (want.size() >= 8)
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->text().startsWith(want, Qt::CaseInsensitive))
                return i;
    return -1;
}

// "(USB Audio Device)" part of a Windows device name: the same for a card's input and output
QString cardPart(const QString &n)
{
    const int open = n.indexOf('(');
    return open >= 0 ? n.mid(open).trimmed() : n.trimmed();
}
} // namespace

// C#: findSound. Lists come from Qt Multimedia (Windows device names, no WinMM index numbers).
void MainWindow::findSound()
{
    const QSignalBlocker b1(ui->audioOutlistBox), b2(ui->audioInlistBox);
    ui->audioOutlistBox->clear();
    ui->audioInlistBox->clear();
    ui->audioOutlistBox->addItems(WsprAudioOut::outputDeviceNames());
    ui->audioInlistBox->addItems(WsprAudioIn::inputDeviceNames());
}

// C#: match_audio_list
void MainWindow::matchAudioList()
{
    const QSignalBlocker b1(ui->audioOutlistBox), b2(ui->audioInlistBox);
    const int o = findName(ui->audioOutlistBox, m_audioOutName);
    ui->audioOutlistBox->setCurrentRow(o);
    ui->AudioOutlabel->setText(o >= 0 ? ui->audioOutlistBox->item(o)->text() : "No output device");
    const int i = findName(ui->audioInlistBox, m_audioInName);
    ui->audioInlistBox->setCurrentRow(i);
    // C# showed the output name here (typo); this shows the input
    ui->AudioInlabel->setText(i >= 0 ? ui->audioInlistBox->item(i)->text() : "No input device");
    ui->AudioOutlabel->adjustSize();
    ui->AudioInlabel->adjustSize();
}

void MainWindow::audioUpdatebutton_Click()
{
    findSound();
    matchAudioList();
}

void MainWindow::audioOutlistBox_SelectedIndexChanged(int row)
{
    if (row < 0)
        return;
    m_audioOutName = ui->audioOutlistBox->item(row)->text();
    ui->AudioOutlabel->setText(m_audioOutName);
    ui->AudioOutlabel->adjustSize();
    if (ui->samecheckBox->isChecked())
        selectSameCard();
}

void MainWindow::audioInlistBox_SelectedIndexChanged(int row)
{
    if (row < 0)
        return;
    m_audioInName = ui->audioInlistBox->item(row)->text();
    ui->AudioInlabel->setText(m_audioInName);
    ui->AudioInlabel->adjustSize();
    if (m_levelMon && m_levelMon->isRunning())
        m_levelMon->start(m_audioInName);   // follow the new choice
}

// C# listed the output devices as inputs when ticked (one WinMM index for both directions).
// Qt keeps inputs and outputs separate, so this picks the input on the same card as the output.
void MainWindow::selectSameCard()
{
    const QString card = cardPart(m_audioOutName);
    if (card.isEmpty())
        return;
    for (int i = 0; i < ui->audioInlistBox->count(); ++i)
        if (cardPart(ui->audioInlistBox->item(i)->text()).compare(card, Qt::CaseInsensitive) == 0) {
            ui->audioInlistBox->setCurrentRow(i);
            return;
        }
}

void MainWindow::samecheckBox_CheckedChanged(bool checked)
{
    if (checked)
        selectSameCard();
}

void MainWindow::trackBarGain_ValueChanged(int value)
{
    m_inLevel = qMax(1, value);
    m_gain = m_inLevel / 100.0f;
    if (m_rx)
        m_rx->setGain(m_gain);   // applies to the recording in progress too
    if (m_levelMon)
        m_levelMon->setGain(m_gain);
    ui->gainlabel->setText(QString::number(m_gain));
    ui->gainlabel->adjustSize();
}

// C#: Monitorbutton - level meter for the selected input
void MainWindow::Monitorbutton_Click()
{
    if (!m_levelMon) {
        m_levelMon = new InputLevelMonitor(this);
        connect(m_levelMon, &InputLevelMonitor::level, this,
                [this](float peak) { ui->volumeMeter1->setValue(int(peak * 100)); });
    }
    m_levelMon->setGain(m_gain);
    if (!m_levelMon->start(m_audioInName))
        Msg::timed(this, m_levelMon->lastError(), "RX audio", 2500);
}

// C#: button5 ("Stop")
void MainWindow::button5_Click()
{
    if (m_levelMon)
        m_levelMon->stop();
}

void MainWindow::wsprdbutton_Click()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Folder containing wsprd", m_wsprdPath);
    if (dir.isEmpty())
        return;
    m_wsprdPath = dir;
    ui->wsprdtextBox->setText(QDir::toNativeSeparators(dir));
#ifdef Q_OS_WIN
    const QString exe = "wsprd.exe";
#else
    const QString exe = "wsprd";
#endif
    if (!QFileInfo::exists(QDir(dir).filePath(exe)))
        Msg::timed(this, "Path does not contain " + exe, "", 3000);
    if (m_rx)
        m_rx->setWsprdPath(m_wsprdPath);
}

bool MainWindow::saveAudio()
{
    if (!m_rx)
        return false;
    RxSettings s;
    s.outputName = m_audioOutName;
    s.outputDevice = ui->audioOutlistBox->currentRow();
    s.inputName = m_audioInName;
    s.inputDevice = ui->audioInlistBox->currentRow();
    s.outLevel = m_outLevel;
    s.inLevel = m_inLevel;
    s.wsprdPath = m_wsprdPath;
    s.sameDevice = ui->samecheckBox->isChecked();
    return m_rx->store().saveRxSettings(s);
}

void MainWindow::SaveAudiobutton_Click()
{
    if (m_rx) {
        m_rx->setGain(m_gain);
        m_rx->setAudioDevice(m_audioInName);   // used from the next cycle
        m_rx->setWsprdPath(m_wsprdPath);
    }
    if (saveAudio())
        Msg::ok(this, "Audio settings saved", "RX audio");
    else
        Msg::ok(this, "Audio settings not saved: " + (m_rx ? m_rx->store().lastError() : QString()), "RX audio");
}
