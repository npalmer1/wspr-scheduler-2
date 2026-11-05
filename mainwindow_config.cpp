// TX Configuration tab: default frequency / offset / power / antenna and "Save Settings".
// C#: defaultpwrcomboBox_SelectedIndexChanged (watts box), SaveAll + SaveConfig (Save Settings), findBaseCall.
//
// Difference from the C# version: a new slot takes the defaults shown on this tab even before they are
// saved (C# used the values from the last Save Settings, so a change looked as if it had been ignored).
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "maidenhead.h"
#include "msg.h"
#include "rxform.h"
#include "rigctld.h"
#include "version.h"

#include <QCheckBox>
#include <QComboBox>
#include <QIntValidator>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QStatusBar>

void MainWindow::setupConfigTab()
{
    // C#: these lists swallowed typing (KeyPress e.Handled = true)
    ui->defaultpwrcomboBox->setEditable(false);
    ui->defaultFcomboBox->setEditable(false);
    ui->defaultOfftextBox->setValidator(new QIntValidator(-20, 220, ui->defaultOfftextBox));   // "(accepts -20 to 220)"
    ui->defaultWtextBox->setReadOnly(true);

    // callsigns in capitals as they are typed; the base call follows the call
    for (QLineEdit *e : {ui->CalltextBox, ui->baseCalltextBox})
        connect(e, &QLineEdit::textEdited, this, [e](const QString &t) {
            if (t == t.toUpper())
                return;
            const int pos = e->cursorPosition();
            e->setText(t.toUpper());
            e->setCursorPosition(pos);
        });
    connect(ui->CalltextBox, &QLineEdit::textChanged, this, [this](const QString &t) {
        const QString base = findBaseCall(t.trimmed().toUpper());
        if (!base.isEmpty())
            ui->baseCalltextBox->setText(base);
    });
    {
        const QSignalBlocker b(ui->defaultpwrcomboBox);
        ui->defaultpwrcomboBox->setCurrentIndex(ui->defaultpwrcomboBox->findText(QString::number(m_cfg.defaultdBm)));
    }
    defaultpwrcomboBox_SelectedIndexChanged(ui->defaultpwrcomboBox->currentIndex());

    ui->asOnecheckBox->setToolTip("Ticked: a compound call (G4GCI/P) or 6-character locator goes out in one\n"
                                  "2-minute slot per transmission.\n"
                                  "Unticked: the older two-transmission way - type 2 (or type 1) in the slot and\n"
                                  "type 3 with the 6-character locator in the next slot (new slots are saved in pairs).");
    Type2checkBox_CheckedChanged(ui->Type2checkBox->isChecked());
    ui->slotTestlistBox->hide();   // Test encoding: the type is decided by the call and settings
    ui->label91->hide();
    connect(ui->asOnecheckBox, &QCheckBox::toggled, this, [this] { showTestSlotChoice(); });
    connect(ui->Type2checkBox, &QCheckBox::toggled, this, [this] { showTestSlotChoice(); });
    showTestSlotChoice();

    // Test encoding: the message type (1-3) and slot (1-2) lists are only a few rows - size them to show
    // every row, no scroll bar (they showed a scroll bar and no numbers at the Qt font size)
    for (QListWidget *l : {ui->slotTestlistBox, ui->slotlistBox}) {
        l->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        l->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        const int rowH = qMax(l->sizeHintForRow(0), l->fontMetrics().height() + 2);
        l->setFixedHeight(rowH * l->count() + 2 * l->frameWidth());
        l->setFixedWidth(qMax(l->width(), l->fontMetrics().horizontalAdvance("88") + 2 * l->frameWidth() + 10));
        if (l->currentRow() < 0)
            l->setCurrentRow(0);
    }
}

// C#: defaultpwrcomboBox_SelectedIndexChanged - watts beside the dBm
void MainWindow::defaultpwrcomboBox_SelectedIndexChanged(int index)
{
    const QString w = index >= 0 ? dBmToWatts(ui->defaultpwrcomboBox->itemText(index).toInt()) : QString();
    ui->defaultWtextBox->setText(w);
}

void MainWindow::takeDefaultsFromUi()
{
    bool ok = false;
    const double f = ui->defaultFcomboBox->currentText().toDouble(&ok);
    if (ok && f > 0.0)
        m_cfg.defaultF = f;
    const int off = ui->defaultOfftextBox->text().trimmed().toInt(&ok);
    if (ok)
        m_cfg.defaultOffset = qBound(0, off, 200);   // a slot offset is 0-200 Hz
    const int dbm = ui->defaultpwrcomboBox->currentText().toInt(&ok);
    if (ok && !dBmToWatts(dbm).isEmpty())
        m_cfg.defaultdBm = dbm;
    if (!ui->DefaultAntcomboBox->currentText().isEmpty())
        m_cfg.defaultAnt = ui->DefaultAntcomboBox->currentText();
}

// C#: findBaseCall - "G4GCI/P" -> "G4GCI", "PJ4/K1ABC" -> "K1ABC", otherwise the call itself
QString MainWindow::findBaseCall(const QString &call) const
{
    const QStringList c = call.split('/');
    if (c.size() < 2)
        return call;
    if (c[0].size() >= 3 && c[1].size() < 4)
        return c[0];
    if (c[0].size() < 3 && c[1].size() >= 3)
        return c[1];
    return QString();
}

// C#: SaveAll / SaveConfig
void MainWindow::saveConfigbutton_Click()
{
    const QString call = ui->CalltextBox->text().trimmed().toUpper();
    const QString loc = ui->LocatortextBox->text().trimmed();
    if (call.isEmpty()) {
        Msg::ok(this, "Callsign cannot be empty");
        return;
    }
    if (!Maidenhead::isValid(loc)) {
        Msg::ok(this, "Invalid locator", "Locator");
        return;
    }
    bool ok = false;
    ui->defaultOfftextBox->text().trimmed().toInt(&ok);
    if (!ok) {
        Msg::ok(this, "Default offset must be a number (0-200 Hz)", "Default offset");
        return;
    }
    int msgT = checkCall(call, loc);
    if (!ui->Type2checkBox->isChecked())
        msgT = 1;
    QString base = ui->baseCalltextBox->text().trimmed().toUpper();
    const QString found = findBaseCall(call);
    if (msgT <= 1) {
        base = found.isEmpty() ? call : found;   // C#: type 1 - base call from the call
    } else if (base.isEmpty()) {
        if (found.isEmpty()) {
            Msg::ok(this, "Enter your base call", "Base call?");
            return;
        }
        base = found;
        Msg::ok(this, "Check if your base call is correct: " + base, "Base call correct?");
    }
    ui->CalltextBox->setText(call);
    ui->baseCalltextBox->setText(base);
    ui->msgTypelabel->setText(QString("Message type %1").arg(qMax(1, msgT)));
    ui->msgTypelabel2->setText(ui->msgTypelabel->text());

    const QString oldLoc4 = m_cfg.locator.left(4).toUpper();
    takeDefaultsFromUi();
    m_cfg.callsign = call;
    m_cfg.baseCall = base;
    m_cfg.locator = loc;
    m_cfg.longLocator = ui->longcheckBox->isChecked();
    m_cfg.allowType2 = ui->Type2checkBox->isChecked();
    m_cfg.oneMsg = ui->asOnecheckBox->isChecked();
    m_cfg.stopRX = ui->stopRXcheckBox->isChecked();
    if (!m_store.saveSettings(m_cfg, qMax(1, msgT))) {
        Msg::ok(this, "Settings not saved: " + m_store.lastError(), "Save Settings");
        return;
    }
    if (m_rx)   // C#: rxForm.updateLabels
        m_rx->setHeader(rxCallsign(), m_store.settings(), loc, m_audioInName, m_wsprdPath, WSPRSKED_VERSION);
    if (loc.left(4).toUpper() != oldLoc4)
        updateSunTimes(true);   // new square: new sunrise/sunset times
    statusBar()->showMessage("Settings saved", 10000);
    Msg::timed(this, "Settings saved", "Save Settings", 3000);
}

// C#: the "Save all settings before exit" answer - SaveAll, SaveRigctl and Save_Audio, without the
// questions Save Settings asks. Returns a line for each part that could not be saved.
QString MainWindow::saveAllSettings()
{
    QStringList problems;
    const QString call = ui->CalltextBox->text().trimmed().toUpper();
    const QString loc = ui->LocatortextBox->text().trimmed();
    if (call.isEmpty() || !Maidenhead::isValid(loc)) {
        problems << "TX Configuration: callsign or locator not valid";
    } else {
        int msgT = checkCall(call, loc);
        if (!ui->Type2checkBox->isChecked())
            msgT = 1;
        takeDefaultsFromUi();
        m_cfg.callsign = call;
        QString base = ui->baseCalltextBox->text().trimmed().toUpper();
        if (base.isEmpty())
            base = findBaseCall(call).isEmpty() ? call : findBaseCall(call);
        m_cfg.baseCall = base;
        m_cfg.locator = loc;
        m_cfg.longLocator = ui->longcheckBox->isChecked();
        m_cfg.allowType2 = ui->Type2checkBox->isChecked();
        m_cfg.oneMsg = ui->asOnecheckBox->isChecked();
        m_cfg.stopRX = ui->stopRXcheckBox->isChecked();
        if (!m_store.saveSettings(m_cfg, qMax(1, msgT)))
            problems << "TX Configuration: " + m_store.lastError();
    }
    const RigctlSettings r = rigctlFromUi();
    if (r.noRigctld || Hamlib::modelNumber(r.radio) > 0) {
        if (m_store.saveRigctl(r))
            m_rigCfg = r;
        else
            problems << "Rigctl rigs: " + m_store.lastError();
    }
    if (!saveAudio())
        problems << "RX & Sound: " + (m_rx ? m_rx->store().lastError() : QString("receive window not open"));
    return problems.join("\n");
}

// Test encoding: "Slot 1 / 2" only means something when type 2/3 go as two transmissions
void MainWindow::showTestSlotChoice()
{
    const bool twoTx = ui->Type2checkBox->isChecked() && !ui->asOnecheckBox->isChecked();
    ui->slotlistBox->setVisible(twoTx);
    ui->label106->setVisible(twoTx);
}
