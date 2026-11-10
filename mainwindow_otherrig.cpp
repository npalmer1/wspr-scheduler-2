// "Non Rigctl TX" tab: transmitters controlled by their own commands instead of rigctld, e.g. a
// self-contained WSPR beacon with a web API, or a rig that takes CAT commands on a COM port.
// C#: ReadRigs / SaveRig / SaveRigInfo / DeleteOtherTX / editrigbutton / addrigbutton / riglistBox /
// setrigBoxes / sendOtherTXRigCommand, and the selectedRig parts of changeFreq and PTT.
//
// In use when "Do not use rigctld" is ticked (Rigctl rigs tab) and a rig is selected here:
//   before each slot (odd :55) the TX command is sent, with  freq call grid pwr offset  filled in
//   start of transmission: the "start TX" command (if any); end: the RX command (if any).
//
// Differences from the C# version:
//  - a failed command stops that transmission (C# showed a pop-up and carried on);
//  - "offset" is filled in for scheduled slots too (C# only replaced it on the default path, and dropped
//    the result there - R.Replace(...) without assigning it);
//  - rigs can be deleted with a right click (C# had the code but no way to reach it safely).
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "othertx.h"
#include "rxspot.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSerialPortInfo>

#include "rigctld.h"
#include <QSignalBlocker>
#include <QStatusBar>
#include <cmath>

namespace {
// the C# texts (Form1.resx label123 / apiinfolabel)
const char *kCommandHelp =
    "Enter the command to set callsign, grid, power (dBm) and frequency:\n"
    "Use the following variables in places of actual values: call, grid, pwr, freq\n"
    "For example: CONFIG:call,grid,pwr,freq\n"
    "Where: call = AZ1AAA, grid = IJ90, pwr = 23, freq = 14097100\n"
    "This example will send: CONFIG:AZ1AAA,IJ90,23,14097100\n"
    "\n"
    "OR commands separated by semi-colon (\";\") eg.:\n"
    "SET FREQ freq; SET POWER pwr; SET LOC grid; SET CALL call; SET OFFSET offset";
const char *kApiHelp =
    "Enter the full API url to be appended to\n"
    "http://<ip_address>:<port>/\n"
    "eg. \"api/cmd?c=\"\n"
    "- to give: http://<ip address>/api/cmd?c=\n"
    "and the TX commands are then added to this, eg.\n"
    "http://<ip address>/api/cmd?c=TXNOW";
} // namespace

void MainWindow::setupOtherRigs()
{
    ui->RiggroupBox->hide();
    ui->rigeditcheckBox->hide();
    ui->label123->setText(kCommandHelp);
    ui->label123->setWordWrap(false);
    ui->label123->adjustSize();
    ui->apiinfolabel->setText(kApiHelp);
    ui->apiinfolabel->adjustSize();
    {   // keep it inside the Rig box
        const int room = ui->RiggroupBox->width() - 8;
        if (ui->apiinfolabel->geometry().right() > room)
            ui->apiinfolabel->move(qMax(0, room - ui->apiinfolabel->width()), ui->apiinfolabel->y());
    }
    if (!m_rigCheckReply) {   // reply check (new): under the RX command
        QWidget *box = ui->RiggroupBox;
        const int y = ui->rigrxcmdtextBox->geometry().bottom() + 14;
        auto *l = new QLabel("Expected reply to TX command:", box);
        l->setGeometry(ui->label136->x(), y + 3, 120, 15);
        l->adjustSize();
        m_rigCheckReply = new QLineEdit(box);
        m_rigCheckReply->setObjectName("rigCheckReplytextBox");
        m_rigCheckReply->setGeometry(l->geometry().right() + 6, y, 200, 20);
        auto *hint = new QLabel("None if empty, otherwise something like \"ok\" or \"band set to\" (upper or lower case doesn't matter).\n"
                                "If the reply doesn't match, the transmission is cancelled.",
                                box);
        hint->setGeometry(m_rigCheckReply->geometry().right() + 10, y + 3, 520, 15);
        hint->adjustSize();
        const QString tip = "If a reply to the TX command doesn't contain this text, that transmission is cancelled.\n"
                            "Leave empty for rigs that don't reply, e.g. Yaesu or Kenwood CAT.";
        for (QWidget *w : std::initializer_list<QWidget *>{l, m_rigCheckReply, hint})
            w->setToolTip(tip);
        if (hint->geometry().right() > box->width() - 8) {   // too wide: put the guidance underneath
            hint->move(ui->label136->x(), y + 24);
        }
        if (box->height() < hint->geometry().bottom() + 10)
            box->resize(box->width(), hint->geometry().bottom() + 10);
    }
    layoutRigEditor();
    for (QComboBox *c : {ui->rigprotocolcomboBox, ui->rigportcomboBox, ui->rigbaudcomboBox, ui->rigdatacomboBox,
                         ui->rigparitycomboBox, ui->rigstopcomboBox, ui->rigflowcomboBox})
        c->setEditable(false);
    for (const char *b : {"19200", "38400", "57600", "115200"})
        if (ui->rigbaudcomboBox->findText(b) < 0)
            ui->rigbaudcomboBox->addItem(b);
    ui->rigporttextBox->setValidator(new QIntValidator(1, 65535, ui->rigporttextBox));
    ui->selrigtextBox->setReadOnly(true);
    ui->riglistBox->setToolTip("Double-click: use this rig.  Right-click: delete it.");
    loadOtherRigList();
}

void MainWindow::loadOtherRigList()
{
    const auto rigs = m_store.loadOtherRigs();
    m_otherRigs = rigs ? *rigs : QVector<OtherRig>();
    m_selectedRig = 0;
    for (const OtherRig &r : std::as_const(m_otherRigs))
        m_selectedRig = r.selected;   // the same in every row
    if (!otherRig(m_selectedRig))
        m_selectedRig = 0;
    const QSignalBlocker b(ui->riglistBox);
    ui->riglistBox->clear();
    ui->riglistBox->addItem("0\tNone specified");
    for (const OtherRig &r : std::as_const(m_otherRigs))
        ui->riglistBox->addItem(QString("%1\t%2").arg(r.id).arg(r.name));
    showSelectedRig();
}

const OtherRig *MainWindow::otherRig(int id) const
{
    for (const OtherRig &r : m_otherRigs)
        if (r.id == id && id > 0)
            return &r;
    return nullptr;
}

void MainWindow::showSelectedRig()
{
    const OtherRig *r = otherRig(m_selectedRig);
    ui->selrigtextBox->setText(r ? QString("%1\t%2").arg(r->id).arg(r->name) : QString("0\tNone specified"));
}

// The help texts are taller on Windows than in the form: put the TX command row under them and move the
// rows below down by the same amount (a little more for the RX command), and keep the API URL box clear
// of its "http://<ip_address>:<port>/" label.
void MainWindow::layoutRigEditor()
{
    QWidget *box = ui->RiggroupBox;
    const int txTop = ui->label123->geometry().bottom() + 8;
    const int delta = txTop - qMin(ui->rigtxcmdtextBox->y(), ui->label124->y());
    if (delta > 0) {
        const int from = qMin(ui->rigtxcmdtextBox->y(), ui->label124->y()) - 2;
        const int rxFrom = qMin(ui->label125->y(), ui->rigrxcmdtextBox->y()) - 2;
        for (QWidget *w : box->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
            if (w != ui->label123 && w->y() >= from)
                w->move(w->x(), w->y() + delta + (w->y() >= rxFrom ? 6 : 0));
        int bottom = 0;
        for (QWidget *w : box->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
            bottom = qMax(bottom, w->geometry().bottom() + 10);
        if (box->height() < bottom)
            box->resize(box->width(), bottom);
    }
    // "Command to start TX" ran under its box
    ui->label134->adjustSize();
    const int pttLeft = ui->label134->geometry().right() + 6;
    if (ui->rigPTTcmdtextBox->x() < pttLeft) {
        const int r = ui->rigPTTcmdtextBox->geometry().right();
        ui->rigPTTcmdtextBox->setGeometry(pttLeft, ui->rigPTTcmdtextBox->y(), r - pttLeft + 1, ui->rigPTTcmdtextBox->height());
    }
    ui->APIstartlabel->adjustSize();
    const int right = ui->APIURLtextBox->geometry().right();
    const int left = ui->APIstartlabel->geometry().right() + 4;
    if (ui->APIURLtextBox->x() < left)
        ui->APIURLtextBox->setGeometry(left, ui->APIURLtextBox->y(), right - left + 1, ui->APIURLtextBox->height());
}

// rigctld unticked and a rig chosen here (C#: noRigctld && selectedRig > 0)
bool MainWindow::otherRigInUse() const
{
    return m_noRigctld && otherRig(m_selectedRig) != nullptr;
}

// C#: setrigBoxes
void MainWindow::setRigBoxes(bool serial)
{
    ui->rigportcomboBox->setVisible(serial);
    ui->rigbaudcomboBox->setVisible(serial);
    ui->rigserialgroupBox->setVisible(serial);
    ui->rigporttextBox->setVisible(!serial);
    ui->rigiptextBox->setVisible(!serial);
    ui->APIgroupBox->setVisible(!serial);
    ui->apiinfolabel->setVisible(!serial);
    ui->rigBPlabel->setText(serial ? "Baud:" : "IP:");
    placeLabelBeside(ui->rigBPlabel, serial ? static_cast<QWidget *>(ui->rigbaudcomboBox) : ui->rigiptextBox);
    placeLabelBeside(ui->rigportlabel, serial ? static_cast<QWidget *>(ui->rigportcomboBox) : ui->rigporttextBox);
    ui->rigBPlabel->setVisible(true);
    ui->rigportlabel->setVisible(true);
    if (serial) {
        const QString keep = ui->rigportcomboBox->currentText();
        const QSignalBlocker b(ui->rigportcomboBox);
        ui->rigportcomboBox->clear();
        ui->rigportcomboBox->addItems(Hamlib::serialPortNames());
        if (!keep.isEmpty() && ui->rigportcomboBox->findText(keep) < 0 && !Hamlib::isPhantomSerialPort(keep))
            ui->rigportcomboBox->addItem(keep);
        ui->rigportcomboBox->setCurrentText(keep);
    }
}

void MainWindow::rigprotocolcomboBox_SelectedIndexChanged(int index)
{
    if (index >= 0)
        setRigBoxes(ui->rigprotocolcomboBox->itemText(index) == "Serial");
}

void MainWindow::httpscheckBox_CheckedChanged(bool checked)
{
    ui->APIstartlabel->setText(checked ? "https://<ip_address>:<port>/" : "http://<ip_address>:<port>/");
}

void MainWindow::openRigEditor(const OtherRig &r, bool isNew)
{
    m_rigEditNew = isNew;
    m_rigEditId = r.id;
    ui->rignolabel->setText(QString::number(r.id));
    ui->rignametextBox->setText(r.name);
    ui->rigtxcmdtextBox->setText(r.txCommand);
    ui->rigrxcmdtextBox->setText(r.rxCommand);
    ui->rigPTTcmdtextBox->setText(r.txPtt);
    m_rigCheckReply->setText(r.checkCommand.isEmpty() ? QString() : r.checkReply);
    ui->APIURLtextBox->setText(r.url.startsWith("http") ? QString() : r.url);
    ui->httpscheckBox->setChecked(r.url.startsWith("https"));
    const bool serial = r.protocol.contains("Serial");
    {
        const QSignalBlocker b(ui->rigprotocolcomboBox);
        const int p = ui->rigprotocolcomboBox->findText(r.protocol.isEmpty() ? QString("TCP") : r.protocol);
        ui->rigprotocolcomboBox->setCurrentIndex(p >= 0 ? p : 1);
    }
    if (serial) {
        ui->rigportcomboBox->setCurrentText(r.port);
    } else {
        ui->rigiptextBox->setText(r.ip);
        ui->rigporttextBox->setText(r.port);
    }
    setRigBoxes(serial);
    if (serial) {
        if (ui->rigportcomboBox->findText(r.port) < 0 && !r.port.isEmpty())
            ui->rigportcomboBox->addItem(r.port);
        ui->rigportcomboBox->setCurrentText(r.port);
        ui->rigbaudcomboBox->setCurrentText(r.baud.isEmpty() ? QString("9600") : r.baud);
        const QStringList s = r.serial.split(',');
        ui->rigdatacomboBox->setCurrentText(s.value(0, "8"));
        ui->rigparitycomboBox->setCurrentText(s.value(1, "none"));
        ui->rigstopcomboBox->setCurrentText(s.value(2, "1"));
        ui->rigflowcomboBox->setCurrentText(s.value(3, "none"));
    }
    ui->RiggroupBox->setTitle(isNew ? "New rig" : "Rig");
    ui->RiggroupBox->show();
    ui->riglistBox->setEnabled(false);
    ui->rignametextBox->setFocus();
}

void MainWindow::addrigbutton_Click()
{
    OtherRig r;
    for (const OtherRig &x : std::as_const(m_otherRigs))
        r.id = qMax(r.id, x.id);
    r.id += 1;
    r.protocol = "TCP";
    r.port = "80";   // a web interface is normally TCP port 80
    openRigEditor(r, true);
}

void MainWindow::editrigbutton_Click()
{
    const int row = ui->riglistBox->currentRow();
    if (row < 1 || row > m_otherRigs.size()) {
        Msg::timed(this, "Select a rig in the list first", "Rig", 2500);
        return;
    }
    openRigEditor(m_otherRigs[row - 1], false);
}

void MainWindow::rigcancelbutton_Click()
{
    ui->RiggroupBox->hide();
    ui->riglistBox->setEnabled(true);
}

// C#: saverigbutton_Click + SaveRig
void MainWindow::saverigbutton_Click()
{
    OtherRig r;
    r.id = m_rigEditId;
    r.name = ui->rignametextBox->text().trimmed();
    if (r.name.isEmpty()) {
        Msg::ok(this, "Rig name is empty");
        return;
    }
    if (ui->rigprotocolcomboBox->currentIndex() < 0) {
        Msg::ok(this, "No protocol selected");
        return;
    }
    r.protocol = ui->rigprotocolcomboBox->currentText();
    if (r.protocol == "Serial") {
        if (ui->rigportcomboBox->currentText().isEmpty() || ui->rigbaudcomboBox->currentIndex() < 0) {
            Msg::ok(this, "Invalid port or baud rate");
            return;
        }
        r.port = ui->rigportcomboBox->currentText();
        r.baud = ui->rigbaudcomboBox->currentText();
        r.serial = QStringList{ui->rigdatacomboBox->currentText(), ui->rigparitycomboBox->currentText(),
                               ui->rigstopcomboBox->currentText(), ui->rigflowcomboBox->currentText()}
                       .join(',');
    } else {
        r.ip = ui->rigiptextBox->text().trimmed();
        r.port = ui->rigporttextBox->text().trimmed();
        if (!checkIP(r.ip) || r.port.toInt() < 1 || r.port.toInt() > 65535) {
            Msg::ok(this, "Invalid IP or port");
            return;
        }
        r.url = ui->APIURLtextBox->text().trimmed();
        while (r.url.startsWith('/'))
            r.url.remove(0, 1);   // C#: "Leading / not needed"
        if (ui->httpscheckBox->isChecked())
            r.url = "https:" + r.url;   // marker read back by openRigEditor / send (C# stored "https..." in URL)
    }
    r.txCommand = ui->rigtxcmdtextBox->text().trimmed();
    r.rxCommand = ui->rigrxcmdtextBox->text().trimmed();
    r.txPtt = ui->rigPTTcmdtextBox->text().trimmed();
    r.checkReply = m_rigCheckReply->text().trimmed();
    r.checkCommand = r.checkReply.isEmpty() ? QString() : QStringLiteral("TX");   // flag: a reply is expected
    r.selected = m_selectedRig;
    if (!m_store.saveOtherRig(r)) {
        Msg::ok(this, "Rig not saved: " + m_store.lastError(), "Rig");
        return;
    }
    rigcancelbutton_Click();
    loadOtherRigList();
    for (int i = 1; i < ui->riglistBox->count(); ++i)
        if (ui->riglistBox->item(i)->text().startsWith(QString::number(r.id) + '\t'))
            ui->riglistBox->setCurrentRow(i);
    Msg::timed(this, "Saved", "Rig", 1000);
}

void MainWindow::riglistBox_MouseClick(QListWidgetItem *) {}

// C#: riglistBox_MouseDoubleClick - "Set this as current rig (Y/N)?"
void MainWindow::riglistBox_MouseDoubleClick(QListWidgetItem *item)
{
    const int row = ui->riglistBox->row(item);
    if (row < 0)
        return;
    const int id = row == 0 ? 0 : m_otherRigs.value(row - 1).id;
    if (!Msg::yesNo(this, "Set this as current rig (Y/N)?", "Rig Name"))
        return;
    m_selectedRig = id;
    if (!m_otherRigs.isEmpty() && !m_store.saveSelectedOtherRig(id))
        Msg::ok(this, "Selection not saved: " + m_store.lastError(), "Rig");
    for (OtherRig &r : m_otherRigs)
        r.selected = id;
    showSelectedRig();
    if (id > 0 && !m_noRigctld)
        Msg::timed(this, "Tick \"Do not use rigctld\" on the Rigctl rigs tab to use this rig", "Rig", 4000);
}

// right click on a rig: delete it
void MainWindow::riglistBox_MouseDown(QMouseEvent *e)
{
    if (e->button() != Qt::RightButton)
        return;
    QListWidgetItem *item = ui->riglistBox->itemAt(e->position().toPoint());
    const int row = item ? ui->riglistBox->row(item) : -1;
    if (row < 1 || row > m_otherRigs.size())
        return;
    const OtherRig r = m_otherRigs[row - 1];
    if (!Msg::yesNo(this, QString("Delete rig %1 \"%2\" (Y/N)?").arg(r.id).arg(r.name), "Delete rig"))
        return;
    if (!m_store.deleteOtherRig(r.id)) {
        Msg::ok(this, "Error deleting TX: " + m_store.lastError());
        return;
    }
    if (m_selectedRig == r.id) {
        m_selectedRig = 0;
        m_store.saveSelectedOtherRig(0);
    }
    loadOtherRigList();
}

// ---- used by transmit -------------------------------------------------------------------------

// C#: the substitutions in changeFreq
QString MainWindow::otherRigCommand(QString cmd, const SlotRecord &s) const
{
    const qint64 txHz = qint64(std::llround(s.freq * 1e6)) + 1400 + s.offset;
    const QString call = (ui->Type2checkBox->isChecked() ? ui->CalltextBox->text() : ui->baseCalltextBox->text())
                             .trimmed()
                             .toUpper();
    QString grid = ui->LocatortextBox->text().trimmed();
    if (!ui->longcheckBox->isChecked())
        grid = grid.left(4);
    cmd.replace("freq", QString::number(txHz));
    cmd.replace("grid", grid);
    cmd.replace("call", call);
    cmd.replace("pwr", QString::number(s.powerdBm));
    cmd.replace("offset", QString::number(s.offset));
    return cmd;
}

// C#: sendOtherTXRigCommand
bool MainWindow::sendOtherRig(const QString &commands, QString *error, bool checkReply)
{
    const OtherRig *r = otherRig(m_selectedRig);
    if (!r) {
        *error = "no rig selected on the Non Rigctl TX tab";
        return false;
    }
    if (commands.trimmed().isEmpty())
        return true;
    OtherTx::RigLink link;
    link.protocol = r->protocol;
    link.ip = r->ip;
    link.port = r->port;
    link.baud = r->baud;
    link.serial = r->serial;
    link.https = r->url.startsWith("https");
    link.apiPath = r->url.startsWith("https:") ? r->url.mid(6) : r->url;
    const auto res = OtherTx::send(link, commands, 3000);
    Rx::debugLog(QString("Rig %1: %2 -> %3").arg(r->name, commands, res.ok ? res.reply : res.error));
    if (!res.ok) {
        *error = res.error;
        return false;
    }
    // optional check (new): a reply to the TX command must contain the text given, any case - e.g. the
    // AD9850 beacon answers "SET FREQ ..." with "Band set to: 20"
    if (!checkReply || r->checkCommand.trimmed().isEmpty() || r->checkReply.trimmed().isEmpty())
        return true;
    const QString want = r->checkReply.trimmed();
    for (const QString &got : res.replies)
        if (got.contains(want, Qt::CaseInsensitive))
            return true;
    *error = QString("no reply contained \"%1\" (the rig said: \"%2\")").arg(want, res.replies.join(" / ").left(120));
    return false;
    return true;
}
