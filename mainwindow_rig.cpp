// "Rigctl rigs" tab: Hamlib rigctld settings, starting/stopping rigctld, the rig list, and reading
// the dial frequency for the receiver (C#: getComports, getRigList, readRigctl, SaveRigctl,
// runRigCtlD, stopRigCtlD, checkRigctld, getRigF and the tab's button handlers).
// Behaviour differences are in rigctld.h and PORTING_NOTES.md.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDoubleSpinBox>
#include <QRegularExpressionValidator>
#include <QIntValidator>
#include <QRegularExpression>
#include <QLabel>

#include "msg.h"
#include "rigctl.h"
#include "rigctld.h"
#include "rxform.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSerialPortInfo>
#include <QTimer>
#include <functional>
#include <memory>

QString MainWindow::hamlibPath() const { return ui->RigCtlPathtextBox->text().trimmed(); }

void MainWindow::setupRig()
{
    if (!m_rigctld) {
        m_rigctld = new RigctldProcess(this);
        connect(m_rigctld, &RigctldProcess::exited, this, [this](int code, const QString &out) {
            // rigctld stopped by itself: usually the COM port is busy or the rig doesn't answer
            setRigLabels(false);
            Msg::ok(this, QString("rigctld stopped (exit code %1)%2").arg(code)
                              .arg(out.isEmpty() ? QString() : ":\n\n" + out.right(600)), "rigctld");
        });
    }
    // C#: the combos are drop-down lists (typing was swallowed by KeyPress handlers)
    ui->RigcomboBox->setEditable(false);
    ui->COMcomboBox->setEditable(false);
    ui->baudcomboBox->setEditable(false);
    // rigctld address and port can be changed (rigctld may run on another PC) - read-only in the C# form
    ui->IPtextBox->setReadOnly(false);
    ui->PorttextBox->setReadOnly(false);
    ui->IPtextBox->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9.:-]{0,64}"), ui->IPtextBox));
    ui->PorttextBox->setValidator(new QIntValidator(1, 65535, ui->PorttextBox));
    ui->IPtextBox->setToolTip("Address of the PC running rigctld: 127.0.0.1 for this PC, or another PC's address or name");
    ui->PorttextBox->setToolTip("rigctld TCP port (normally 4532)");

    QString path = m_cfg.hamlibPath;
    if (path.isEmpty())
        path = "C:/WSPR_Sked";   // C#: HamlibPath default
    ui->RigCtlPathtextBox->setText(QDir::toNativeSeparators(path));

    getComports();
    if (const auto r = m_store.loadRigctl())
        m_rigCfg = *r;
    m_noRigctld = m_rigCfg.noRigctld;
    applyRigctlToUi();

    if (!m_noRigctld) {
        getRigList();
        if (RigctldProcess::isAnswering(m_rigCfg.ipv4, m_rigCfg.port.toInt(), 500))
            setRigLabels(true);   // already running (e.g. started by the C# app)
        else
            startRigctld(false);
    } else {
        setRigLabels(false);
    }
}

// C#: getComports - also fills the Switches tab's port list
void MainWindow::getComports()
{
    QStringList ports;
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts())
        ports << p.portName();
    ports.sort();
    for (QComboBox *c : {ui->COMcomboBox, ui->HwPortcomboBox}) {
        const QString keep = c->currentText();
        const QSignalBlocker b(c);
        c->clear();
        c->addItems(ports);
        if (!keep.isEmpty() && c->findText(keep) < 0)
            c->addItem(keep);   // saved port not plugged in: keep it visible
        c->setCurrentIndex(c->findText(keep));
    }
}

// C#: getRigList - "rigctl -l" into the radio list, keeping the selection
void MainWindow::getRigList()
{
    const int want = Hamlib::modelNumber(ui->RigcomboBox->currentText().isEmpty() ? m_rigCfg.radio
                                                                                   : ui->RigcomboBox->currentText());
    QString err;
    const QStringList rigs = Hamlib::rigList(hamlibPath(), &err);
    const QSignalBlocker b(ui->RigcomboBox);
    ui->RigcomboBox->clear();
    ui->RigcomboBox->addItems(rigs);
    int sel = -1;
    for (int i = 0; i < rigs.size(); ++i)
        if (Hamlib::modelNumber(rigs[i]) == want && want > 0) {
            sel = i;
            break;
        }
    if (sel < 0 && !m_rigCfg.radio.isEmpty()) {   // list unavailable: still show the saved radio
        ui->RigcomboBox->addItem(m_rigCfg.radio);
        sel = ui->RigcomboBox->count() - 1;
    }
    ui->RigcomboBox->setCurrentIndex(sel);
    if (rigs.isEmpty() && !err.isEmpty())
        Msg::timed(this, err, "Rig list", 3000);
}

// " 1046  Yaesu  FT-450D  20230328.2  Stable  RIG_MODEL_FT450" -> "Yaesu FT-450D (Hamlib model 1046)"
QString MainWindow::radioDisplayName(const QString &line)
{
    const QStringList t = line.simplified().split(' ', Qt::SkipEmptyParts);
    const int model = Hamlib::modelNumber(line);
    if (t.size() < 2 || model <= 0)
        return line.simplified();
    QStringList name;
    static const QRegularExpression version("^\\d{6,8}(\\.\\d+)?$");
    for (int i = 1; i < t.size(); ++i) {
        if (version.match(t[i]).hasMatch() || t[i].startsWith("RIG_MODEL"))
            break;
        name << t[i];
    }
    return QString("%1 (Hamlib model %2)").arg(name.join(' ')).arg(model);
}

// C# label31 "Radio to use:" sat off to the right with nothing after it; now under "Stop RigCtlD",
// with the radio that is saved and used
void MainWindow::showRadioToUse()
{
    QLabel *l = ui->label31;
    const QRect stop = m_rigTestButton ? m_rigTestButton->geometry() : ui->stopRigCtlDbutton->geometry();
    QString radio;
    if (m_rigCfg.noRigctld)
        radio = "none (rigctld not used)";
    else if (m_rigCfg.radio.trimmed().isEmpty())
        radio = "none selected";
    else
        radio = radioDisplayName(m_rigCfg.radio);
    l->setText("Radio to use: " + radio);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
    l->adjustSize();
    l->move(stop.x(), stop.bottom() + 10);
}

void MainWindow::applyRigctlToUi()
{
    const QSignalBlocker b1(ui->rigctldcheckBox), b2(ui->VoxcheckBox);
    if (!m_rigCfg.radio.isEmpty() && ui->RigcomboBox->findText(m_rigCfg.radio) < 0)
        ui->RigcomboBox->addItem(m_rigCfg.radio);
    ui->RigcomboBox->setCurrentIndex(ui->RigcomboBox->findText(m_rigCfg.radio));
    if (!m_rigCfg.comPort.isEmpty() && ui->COMcomboBox->findText(m_rigCfg.comPort) < 0)
        ui->COMcomboBox->addItem(m_rigCfg.comPort);
    ui->COMcomboBox->setCurrentIndex(ui->COMcomboBox->findText(m_rigCfg.comPort));
    ui->baudcomboBox->setCurrentIndex(ui->baudcomboBox->findText(m_rigCfg.baud));
    ui->IPtextBox->setText(m_rigCfg.ipv4.isEmpty() ? QString("127.0.0.1") : m_rigCfg.ipv4);
    ui->PorttextBox->setText(m_rigCfg.port.isEmpty() ? QString("4532") : m_rigCfg.port);
    ui->rigctldcheckBox->setChecked(m_rigCfg.noRigctld);
    ui->VoxcheckBox->setChecked(m_rigCfg.vox);
    ensurePpmBox();
    showRadioToUse();
    {
        const QSignalBlocker b3(m_ppmBox);
        m_ppmBox->setValue(m_rigCfg.ppm);
    }
    // C#: manual frequency list shown when rigctld is off (and no non-rigctl rig is selected)
    ui->FlistBox2->setVisible(m_noRigctld);
    ui->Flabel->setVisible(m_noRigctld);
    ui->Fhelplabel->setVisible(m_noRigctld);
}

// Rig frequency correction (new): created here, not in the .ui, so the C# form layout is unchanged
void MainWindow::ensurePpmBox()
{
    if (m_ppmBox)
        return;
    auto *label = new QLabel("Frequency correction (ppm):", ui->tabPage3);
    label->setGeometry(25, 309, 150, 15);
    m_ppmBox = new QDoubleSpinBox(ui->tabPage3);
    m_ppmBox->setObjectName("ppmspinBox");
    m_ppmBox->setRange(-50.0, 50.0);
    m_ppmBox->setDecimals(2);
    m_ppmBox->setSingleStep(0.05);
    m_ppmBox->setGeometry(175, 305, 70, 22);
    const QString tip = "Makes up for a rig whose reference oscillator is a little off.\n"
                        "If wsprnet reports you X Hz low at F MHz, enter X / F (e.g. 12 Hz low at 14.097 MHz = 0.85).\n"
                        "Positive tunes the rig higher. Applies to transmit and to the received dial frequency.\n"
                        "Saved with \"Save rigctl settings\".";
    label->setToolTip(tip);
    m_ppmBox->setToolTip(tip);
    connect(m_ppmBox, &QDoubleSpinBox::valueChanged, this, [this](double v) { m_rigCfg.ppm = v; });   // used from the next slot
    label->show();
    m_ppmBox->show();
}

RigctlSettings MainWindow::rigctlFromUi() const
{
    RigctlSettings r;
    r.radio = ui->RigcomboBox->currentText();
    r.comPort = ui->COMcomboBox->currentText();
    r.baud = ui->baudcomboBox->currentText();
    r.ipv4 = ui->IPtextBox->text().trimmed();
    r.port = ui->PorttextBox->text().trimmed();
    r.noRigctld = ui->rigctldcheckBox->isChecked();
    r.vox = ui->VoxcheckBox->isChecked();
    r.ppm = m_ppmBox ? m_ppmBox->value() : m_rigCfg.ppm;
    return r;
}

void MainWindow::setRigLabels(bool running)
{
    m_rigRunning = running;
    const QString t = running ? "rigctld running" : "rigctld not running";
    for (QLabel *l : {ui->rigrunlabel, ui->Riglabel, ui->Riglabel1}) {
        l->setText(t);
        l->adjustSize();
    }
}

void MainWindow::startRigctld(bool announce)
{
    const QString ip = m_rigCfg.ipv4.trimmed().isEmpty() ? QString("127.0.0.1") : m_rigCfg.ipv4.trimmed();
    const int port = m_rigCfg.port.toInt() > 0 ? m_rigCfg.port.toInt() : 4532;
    if (!Hamlib::isLocalAddress(ip)) {
        // rigctld on another PC: connect to it, don't start one here
        if (!m_rig)
            m_rig = std::make_unique<RigCtl>();
        m_rig->setAddress(ip, port);
        const bool ok = RigctldProcess::isAnswering(ip, port, 1500);
        setRigLabels(ok);
        if (ok)
            getRigF();
        if (announce)
            Msg::timed(this, ok ? QString("Connected to rigctld on %1:%2").arg(ip).arg(port)
                                : QString("rigctld on %1:%2 is not answering - it runs on that PC, start it there "
                                          "(with -T 0.0.0.0 or that PC's address)").arg(ip).arg(port),
                       "rigctld", 4000);
        return;
    }
    if (!m_rigctld->start(hamlibPath(), m_rigCfg)) {
        setRigLabels(false);
        Msg::timed(this, "Unable to start rigctld: " + m_rigctld->error(), "rigctld", 3000);
        return;
    }
    if (!m_rig)
        m_rig = std::make_unique<RigCtl>();
    m_rig->setAddress(m_rigCfg.ipv4.isEmpty() ? QString("127.0.0.1") : m_rigCfg.ipv4,
                      m_rigCfg.port.toInt() > 0 ? m_rigCfg.port.toInt() : 4532);
    // C# waited 2 s and then looked for a process; this asks rigctld for the frequency, once a second
    // for up to 8 s (opening the rig can take a few seconds, more with a slow CAT rate)
    auto tries = std::make_shared<int>(0);
    auto check = std::make_shared<std::function<void()>>();
    *check = [this, announce, tries, check] {
        const bool ok = RigctldProcess::isAnswering(m_rigCfg.ipv4, m_rigCfg.port.toInt(), 800);
        if (!ok && ++*tries < 8 && m_rigctld->startedHere()) {
            QTimer::singleShot(1000, this, [check] { (*check)(); });
            return;
        }
        setRigLabels(ok);
        if (ok) {
            getRigF();
            if (announce)
                Msg::timed(this, "RigCtlD started successfully", "", 3000);
        } else if (m_rigctld->startedHere()) {
            // e.g. wrong COM port or baud rate: rigctld keeps running but can't reach the rig
            QString out = m_rigctld->lastOutput().trimmed();
            const int i = out.indexOf("rig_open");
            if (i >= 0)
                out = out.mid(i);
            Msg::ok(this, "rigctld is running but the rig is not answering - check the radio, COM port and baud rate."
                          + (out.isEmpty() ? QString() : "\n\nrigctld said:\n" + out.left(400)), "rigctld");
        }
    };
    QTimer::singleShot(1500, this, [check] { (*check)(); });
}

// C#: getRigF - every 30 s (seconds 2 and 38) and after rigctld starts
void MainWindow::getRigF()
{
    if (m_noRigctld) {
        // C#: FlistBox2 selection, else the default frequency
        m_dialMHz = QString::number(m_manualMHz > 0.0 ? m_manualMHz : m_cfg.defaultF, 'f', 6);
        if (m_rx)
            m_rx->setFrequency(m_dialMHz);
        return;
    }
    if (!m_rig) {
        m_rig = std::make_unique<RigCtl>();
        m_rig->setAddress(m_rigCfg.ipv4.isEmpty() ? QString("127.0.0.1") : m_rigCfg.ipv4,
                          m_rigCfg.port.toInt() > 0 ? m_rigCfg.port.toInt() : 4532);
    }
    m_rig->setTimeout(700);   // on the UI thread: never hold it up for long
    const auto hz = m_rig->frequency();
    if (!hz || *hz <= 0) {
        if (m_rigRunning)
            setRigLabels(false);
        return;
    }
    if (!m_rigRunning)
        setRigLabels(true);
    // the dial is tuned m_rigCfg.ppm high to make up for the rig's reference: report what is received
    m_dialMHz = QString::number(actualFromDialHz(*hz, m_rigCfg.ppm) / 1e6, 'f', 6);
    if (m_rx)
        m_rx->setFrequency(m_dialMHz);
}

// ---- tab buttons --------------------------------------------------------------------------

void MainWindow::comRbutton_Click()
{
    getComports();
    getRigList();
}

void MainWindow::rigsearchbutton_Click()
{
    const QString search = ui->rigtextBox->text().trimmed();
    if (search.isEmpty())
        return;
    for (int i = 0; i < ui->RigcomboBox->count(); ++i)
        if (ui->RigcomboBox->itemText(i).contains(search, Qt::CaseInsensitive)) {
            ui->RigcomboBox->setCurrentIndex(i);
            return;
        }
    Msg::timed(this, "No rig matching \"" + search + "\"", "Rig search", 2000);
}

void MainWindow::rigctldcheckBox_CheckedChanged(bool checked)
{
    m_noRigctld = checked;
    m_rigCfg.noRigctld = checked;
    ui->FlistBox2->setVisible(checked);
    ui->Flabel->setVisible(checked);
    ui->Fhelplabel->setVisible(checked);
    if (checked) {
        setRigLabels(false);
        getRigF();   // receiver falls back to the default frequency
        if (const OtherRig *r = otherRig(m_selectedRig))   // C#: "Using rig selected from Non rigctl TX tab"
            Msg::timed(this, "Using rig \"" + r->name + "\" selected on the Non Rigctl TX tab", "Rigctld disabled", 3500);
    } else {
        getRigList();
        m_rigCfg = rigctlFromUi();
        startRigctld(false);
    }
}

void MainWindow::VoxcheckBox_CheckedChanged(bool checked)
{
    m_rigCfg.vox = checked;   // used by transmit (no PTT command when ticked); saved with the rest
}

void MainWindow::RigCtlPbutton_Click()
{
    const QString dir = QFileDialog::getExistingDirectory(this, "Hamlib folder (containing rigctld)", hamlibPath());
    if (dir.isEmpty())
        return;
    ui->RigCtlPathtextBox->setText(QDir::toNativeSeparators(dir));
    if (Hamlib::executable(dir, "rigctld").isEmpty()
        || !QFileInfo(Hamlib::executable(dir, "rigctld")).absolutePath().startsWith(QDir(dir).absolutePath(), Qt::CaseInsensitive))
        Msg::ok(this, "rigctld daemon not in this folder");
}

void MainWindow::SaveRPathbutton_Click()
{
#ifdef Q_OS_WIN
    const QString exe = "rigctld.exe";
#else
    const QString exe = "rigctld";
#endif
    if (!QFileInfo::exists(QDir(hamlibPath()).filePath(exe))) {
        Msg::ok(this, exe + " daemon not in this folder");
        return;
    }
    // C# called SaveAll (every setting); only the path is saved here
    if (m_store.saveHamlibPath(m_cfg.configId, hamlibPath())) {
        m_cfg.hamlibPath = hamlibPath();
        Msg::timed(this, "Path saved", "", 1500);
    } else {
        Msg::ok(this, "Path not saved: " + m_store.lastError());
    }
}

void MainWindow::startRigCtlbutton_Click()
{
    if (ui->rigctldcheckBox->isChecked()) {
        Msg::timed(this, "RigCtlD is not enabled", "", 2000);
        return;
    }
    m_rigCfg = rigctlFromUi();
    startRigctld(true);
}

void MainWindow::stopRigCtlDbutton_Click()
{
    const bool stopped = m_rigctld && m_rigctld->stop();
    Msg::timed(this, stopped ? "Stopped RigCtlD" : "RigCtlD not running", "", stopped ? 3000 : 2000);
    setRigLabels(false);
}

void MainWindow::SaveRigctlbutton_Click()
{
    const RigctlSettings r = rigctlFromUi();
    if (!r.noRigctld && (r.ipv4.isEmpty() || r.port.toInt() < 1 || r.port.toInt() > 65535)) {
        Msg::ok(this, "Enter the rigctld IP address (127.0.0.1 for this PC) and a TCP port (1-65535)", "rigctld");
        return;
    }
    const bool remote = !Hamlib::isLocalAddress(r.ipv4);
    if (!r.noRigctld && !remote && (Hamlib::modelNumber(r.radio) <= 0 || r.comPort.isEmpty() || r.baud.isEmpty())) {
        Msg::ok(this, "Error: some items not selected");
        return;
    }
    if (!m_store.saveRigctl(r)) {
        Msg::ok(this, "Unable to save settings: " + m_store.lastError());
        return;
    }
    m_rigCfg = r;
    showRadioToUse();
    Msg::timed(this, "Settings saved", "rigctld", 2000);
    if (!r.noRigctld)
        startRigctld(true);   // C#: runs rigctld with the new settings
}
