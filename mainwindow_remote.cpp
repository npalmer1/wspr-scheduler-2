// Remote tab: switch TX and the schedule on or off from a browser, and (if allowed) pick a frequency and
// antenna and use the next slot. C#: RemoteTabWiring.Install + the Remote* callbacks in Form1.
//
// Settings are kept in <home>/WSPRSkedRemote/remote.ini with the certificate (remote.pfx). The password
// and passcode are stored only as salted PBKDF2 hashes, so the boxes are left empty after Save.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "rigctl.h"
#include "rxspot.h"
#include "txplan.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStatusBar>
#include <cmath>

void MainWindow::setupRemote()
{
    Remote::loadConfig(Remote::configDir(), &m_remoteCfg);
    ui->remoteEnabledCheckBox->setChecked(m_remoteCfg.enabled);
    ui->remoteAllowTxCheckBox->setChecked(m_remoteCfg.allowTxRx);
    ui->remotePortTextBox->setValidator(new QIntValidator(1024, 65535, ui->remotePortTextBox));
    ui->remotePortTextBox->setText(QString::number(m_remoteCfg.port));
    ui->remotePortTextBox->setToolTip("1024-65535, for example 5029");
    ui->remoteUserTextBox->setMaxLength(32);
    ui->remoteUserTextBox->setText(m_remoteCfg.user);
    for (QLineEdit *e : {ui->remotePasswordTextBox, ui->remotePasscodeTextBox}) {
        e->setMaxLength(64);
        e->clear();
        e->setPlaceholderText(m_remoteCfg.hasSecrets() ? "(unchanged)" : QString());
    }
    ui->remotePasswordTextBox->setToolTip("At least 8 characters");
    ui->remotePasscodeTextBox->setToolTip("At least 4 characters - can be numbers only");
    showcodecheckBox_CheckedChanged(ui->showcodecheckBox->isChecked());
    connect(ui->remoteStatusButton, &QPushButton::clicked, this, &MainWindow::showRemoteStatus);

    for (QLabel *l : {ui->remoteStatusLabel, ui->label147, ui->label148}) {
        l->setWordWrap(true);
        l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    // fixed sizes: the start-up label sizing shrinks these to one empty line
    ui->remoteStatusLabel->resize(560, 40);
    ui->label147->resize(620, 80);
    ui->label148->setGeometry(ui->label148->x(), ui->label147->geometry().bottom() + 12, 640, 100);
    ui->remoteInfo->setWordWrap(true);

    if (m_remoteCfg.enabled) {
        QString err;
        if (!startRemote(&err)) {
            m_remoteError = err;
            Rx::debugLog("Remote: not started: " + err);
            statusBar()->showMessage("Remote access not started: " + err, 30000);
        }
    }
    showRemoteStatus();
}

// C#: showcodecheckBox_CheckedChanged
void MainWindow::showcodecheckBox_CheckedChanged(bool checked)
{
    for (QLineEdit *e : {ui->remotePasswordTextBox, ui->remotePasscodeTextBox})
        e->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

bool MainWindow::startRemote(QString *error)
{
    m_remoteError.clear();
    if (!m_remoteCfg.hasSecrets() || m_remoteCfg.user.isEmpty()) {
        *error = "no username, password or passcode saved";
        return false;
    }
    const QString pfx = QDir(Remote::configDir()).filePath("remote.pfx");
    if (!QFile::exists(pfx)) {
        ui->remoteStatusLabel->setText("Making a certificate for the remote page - please wait...");
        ui->remoteStatusLabel->repaint();
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QSslConfiguration ssl;
    const bool certOk = Remote::loadOrCreateCertificate(Remote::configDir(), &m_remoteCfg, &ssl, error);
    QApplication::restoreOverrideCursor();
    if (!certOk)
        return false;

    if (!m_remote) {
        m_remote = new RemoteServer(this);
        RemoteCallbacks cb;
        cb.title = [this] {
            const QString call = ui->CalltextBox->text().trimmed().toUpper();
            return call.isEmpty() ? QString("WSPR Scheduler") : "WSPR Scheduler - " + call;
        };
        cb.status = [this] { return remoteStatus(); };
        // C#: RemoteEnableTx / RemoteDisableTx / RemoteDisableSked / RemoteEnableSked
        cb.enableTx = [this] {
            ui->enableTXcheckBox->setChecked(true);
            return "TX enabled at " + QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " UTC"
                   + (isDryRun() ? " (dry run is on: the rig won't be keyed)" : QString());
        };
        cb.disableTx = [this] {
            ui->enableTXcheckBox->setChecked(false);   // stops a transmission in progress too
            m_slotActive = false;                      // a queued Go next slot still tunes, but only receives
            if (m_txTest == TxTest::Scheduled)
                m_txTest = TxTest::None;
            return "TX disabled at " + QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " UTC";
        };
        cb.disableSked = [this] {
            ui->noSkedcheckBox->setChecked(true);
            return "Schedule disabled at " + QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " UTC";
        };
        cb.enableSked = [this] {
            ui->noSkedcheckBox->setChecked(false);
            return "Schedule enabled at " + QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " UTC";
        };
        cb.txEnabled = [this] { return ui->enableTXcheckBox->isChecked(); };
        cb.skedEnabled = [this] { return !ui->noSkedcheckBox->isChecked(); };
        // C#: RemoteGetFrequencies (FlistBox) / RemoteGetAntennas ("AntNo: AntName")
        cb.frequencies = [this] {
            QStringList l;
            for (int i = 0; i < ui->FlistBox->count(); ++i)
                l << ui->FlistBox->item(i)->text().trimmed();
            return l;
        };
        cb.antennas = [this] {
            QStringList l;
            for (const AntennaInfo &a : m_antennas)
                l << QString("%1: %2").arg(a.antNo).arg(a.name.trimmed());
            return l;
        };
        cb.txNext = [this](const QString &f, const QString &a) { return remoteTxNext(f, a); };
        m_remote->setCallbacks(cb);
        connect(m_remote, &RemoteServer::activity, this, [this](const QString &line) {
            m_remoteLog << line;
            while (m_remoteLog.size() > 4)
                m_remoteLog.removeFirst();
            Rx::debugLog("Remote: " + line);
            statusBar()->showMessage("Remote: " + line, 30000);
            showRemoteStatus();
        });
    }
    if (!m_remote->start(m_remoteCfg, ssl, error))
        return false;
    Rx::debugLog(QString("Remote: listening on port %1").arg(m_remoteCfg.port));
    return true;
}

// C#: remoteSaveButton_Click (empty in C#)
void MainWindow::remoteSaveButton_Click()
{
    RemoteConfig c = m_remoteCfg;
    c.enabled = ui->remoteEnabledCheckBox->isChecked();
    c.allowTxRx = ui->remoteAllowTxCheckBox->isChecked();
    if (!ui->remotePortTextBox->hasAcceptableInput()) {
        Msg::ok(this, "Port must be a number from 1024 to 65535 (for example 5029)", "Remote");
        return;
    }
    c.port = ui->remotePortTextBox->text().toInt();
    c.user = ui->remoteUserTextBox->text().trimmed();
    const QString pw = ui->remotePasswordTextBox->text();
    const QString code = ui->remotePasscodeTextBox->text();
    if (!pw.isEmpty() && pw.size() < 8) {
        Msg::ok(this, "The password must be at least 8 characters", "Remote");
        return;
    }
    if (!code.isEmpty() && code.size() < 4) {
        Msg::ok(this, "The passcode must be at least 4 characters", "Remote");
        return;
    }
    if (!pw.isEmpty())
        Remote::setSecret(&c.passwordSalt, &c.passwordHash, pw);
    if (!code.isEmpty())
        Remote::setSecret(&c.passcodeSalt, &c.passcodeHash, code);
    if (c.enabled && (c.user.isEmpty() || !c.hasSecrets())) {
        Msg::ok(this, "Enter a username, password and passcode for remote access", "Remote");
        return;
    }
    QString err;
    if (!Remote::saveConfig(Remote::configDir(), c, &err)) {
        Msg::ok(this, "Remote settings not saved: " + err, "Remote");
        return;
    }
    m_remoteCfg = c;
    for (QLineEdit *e : {ui->remotePasswordTextBox, ui->remotePasscodeTextBox}) {
        e->clear();
        e->setPlaceholderText(c.hasSecrets() ? "(unchanged)" : QString());
    }

    QString msg = "Remote settings saved";
    if (c.enabled) {
        // restarted every time: a new password logs everyone out
        if (startRemote(&err)) {
            msg += QString(" - remote access on port %1").arg(c.port);
        } else {
            m_remoteError = err;
            showRemoteStatus();
            Msg::ok(this, "Remote settings saved, but remote access is not running:\n\n" + err, "Remote");
            return;
        }
    } else {
        if (m_remote)
            m_remote->stop();
        msg += " - remote access off";
    }
    showRemoteStatus();
    Msg::timed(this, msg, "Remote", 3000);
}

void MainWindow::showRemoteStatus()
{
    const bool running = m_remote && m_remote->isListening();
    QString status, colour, info;
    if (!m_remoteCfg.enabled) {
        status = "Remote access is off.";
        colour = "gray";
    } else if (running) {
        status = QString("Remote access running on port %1 (TX/RX control %2). "
                         "Forward this port on your router to reach it from outside.")
                     .arg(m_remote->port())
                     .arg(m_remoteCfg.allowTxRx ? "allowed" : "not allowed");
        colour = "darkgreen";
        info = "From a browser on this network open:\n   " + Remote::localUrls(m_remote->port()).join("\n   ")
               + "\nThe browser warns about the certificate the first time - choose Advanced, then carry on to the page.";
        if (m_remote->sessionCount() > 0)
            info += QString("\nLogged in now: %1").arg(m_remote->sessionCount());
    } else {
        status = "Remote access not running: " + (m_remoteError.isEmpty() ? QString("press Save") : m_remoteError);
        colour = "red";
    }
    ui->remoteStatusLabel->setText(status);
    ui->remoteStatusLabel->setStyleSheet(QString("QLabel#remoteStatusLabel { color: %1; }").arg(colour));
    ui->label147->setText(info);
    ui->label148->setText(m_remoteLog.isEmpty() ? QString() : "Last remote activity (UTC):\n" + m_remoteLog.join("\n"));
}

// Status box on the remote page (C#: RemoteStatus, with more detail)
QString MainWindow::remoteStatus()
{
    const QDateTime utc = QDateTime::currentDateTimeUtc();
    QStringList l;
    l << utc.toString("HH:mm:ss") + " UTC  " + utc.toString("ddd dd MMM yyyy");
    const bool tx = ui->enableTXcheckBox->isChecked();
    l << QString("TX: %1").arg(tx ? (isDryRun() ? "ENABLED - dry run (the rig is not keyed)" : "ENABLED") : "disabled");
    if (m_transmitting && m_txSlot)
        l << QString("Transmitting now on %1 MHz").arg(m_txSlot->freq, 0, 'f', 4);
    l << QString("Schedule: %1").arg(ui->noSkedcheckBox->isChecked() ? "disabled" : "enabled");
    l << "State: " + ui->TXrunbutton->text();
    if (!ui->countdownlabel->text().isEmpty())
        l << ui->countdownlabel->text();
    const QString ant = m_txAntenna.isEmpty() ? ui->TXRXAntlabel->text() : m_txAntenna;
    if (!ant.isEmpty())
        l << "Antenna: " + ant;
    if (m_stopRX)
        l << "Receive decoding: off";
    if (!m_remoteTxStatus.isEmpty())
        l << "Last Go next slot: " + m_remoteTxStatus;
    return l.join("\n");
}

// C#: RemoteTxNextSlot - the next 2-minute slot on this frequency and antenna, with the default offset
// and power. Transmits if TX is enabled (and the band isn't receive only), otherwise receives.
// The schedule is disabled, as with "Disable sked".
QString MainWindow::remoteTxNext(const QString &freqMHz, const QString &antenna)
{
    bool ok = false;
    const double f = freqMHz.toDouble(&ok);
    if (!ok || f <= 0.0)
        return "Invalid frequency";

    // C#: resolveAntenna - "2: Vertical", a number, or a name
    const QString in = antenna.trimmed();
    const int colon = in.indexOf(':');
    const QString num = colon > 0 ? in.left(colon).trimmed() : in;
    const QString name = colon > 0 ? in.mid(colon + 1).trimmed() : in;
    const AntennaInfo *ant = nullptr;
    const int no = num.toInt(&ok);
    for (const AntennaInfo &a : m_antennas)
        if ((ok && a.antNo == no) || (!ant && a.name.trimmed().compare(name, Qt::CaseInsensitive) == 0))
            ant = &a;
    if (!ant)
        return "Unknown antenna: " + antenna;

    if (m_transmitting)
        return "Transmitting now - try again after this slot";
    if (!m_noRigctld && !txRig()->isReachable())
        return "rigctld not running - nothing changed";
    if (ui->CalltextBox->text().trimmed().isEmpty())
        return "No callsign set - nothing changed";

    bool txOn = ui->enableTXcheckBox->isChecked();
    const bool rxOnly = TxPlan::isReceiveOnly(f);
    if (txOn && rxOnly)
        txOn = false;   // 22 m / 8 m: listen only

    ui->noSkedcheckBox->setChecked(true);   // same as "Disable sked"
    const bool handTuned = manualFrequencyMode();
    if (handTuned)
        selectManualFrequency(f);           // C#: FlistBox2.SelectedItem - the receiver labels spots with it

    SlotRecord s;
    s.freq = f;
    bool offOk = false;
    const int off = ui->defaultOfftextBox->text().trimmed().toInt(&offOk);
    s.offset = qBound(0, offOk ? off : m_cfg.defaultOffset, 200);
    s.powerdBm = m_cfg.defaultdBm;
    bool pOk = false;
    const int p = ui->defaultpwrcomboBox->currentText().trimmed().toInt(&pOk);
    if (pOk)
        s.powerdBm = p;
    s.antenna = ant->name;
    s.active = txOn;
    s.slotNo = 1;
    s.msgType = ui->Type2checkBox->isChecked() ? 3 : 1;   // C#: msgType = Type2checkBox.Checked ? 3 : 1

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime cycle = TxPlan::nextCycleStart(now);
    if (now.secsTo(cycle) < 4)
        cycle = cycle.addSecs(120);   // too late to switch and tune for this one
    m_txSlot = s;
    m_txCycleUtc = cycle;
    m_slotActive = txOn;
    m_prepDone = false;
    m_fired = false;
    m_txTest = TxTest::Remote;
    m_previewCycle = QDateTime();
    m_preview.reset();
    ui->mtypelabel->setText(QString::number(s.msgType));
    ui->slotnolabel->setText("1");

    const bool lt = ui->LTcheckBox->isChecked();
    const QString at = (lt ? cycle.toLocalTime() : cycle).toString("HH:mm") + (lt ? " LT" : " UTC");
    const QString where = QString(" on %1 MHz, antenna %2").arg(f, 0, 'f', 4).arg(ant->name.trimmed());
    if (txOn)
        m_remoteTxStatus = "TX armed for " + at + where + (isDryRun() ? " (dry run)" : QString());
    else
        m_remoteTxStatus = "RX from " + at + where + (rxOnly ? " (RX-only band)" : " (TX disabled)");
    if (handTuned)
        m_remoteTxStatus += " - rigctld is off: the rig must already be on this frequency";
    txStatus("Remote: " + m_remoteTxStatus);
    showTxState(now);
    return m_remoteTxStatus;
}
