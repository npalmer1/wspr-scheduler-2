// Freq&Ant tab: antennas (name, description, switch + port, second switch + port, tuner) and the
// WSPR frequency list with the TX audio level for each frequency
// (C#: ReadAntennas list part, SaveAntenna, DeleteAntenna, UpdateSlotAntennas, ReadFrequencies,
// SaveFrequency, DeleteFrequency, addAnt/editAnt/delAnt/saveAnt, editF/saveFreq and the tab's handlers).
//
// Differences from the C# version:
//  - a new antenna gets the next free number (C# used the list length, so after a delete the new one
//    could take an existing antenna's number and overwrite it);
//  - deleting an antenna also removes it from the antenna drop-downs (C# tried to remove the whole
//    list line from them, which never matched, so the deleted antenna stayed selectable);
//  - frequencies added earlier are shown again after a restart (C# only updated the standard ones
//    already in the list, so added frequencies vanished from it although they were in the database);
//  - all values are bound, so names with an apostrophe save (C# UPDATE pasted them into the SQL);
//  - the frequency box only accepts numbers (C# popped up "Invalid frequency" on every bad key);
//  - list columns are lined up with spaces under the headings (C# used tab stops, which Qt lists don't have).
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"
#include "wspraudioout.h"

#include <QDoubleValidator>
#include <QLabel>
#include <QListWidget>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <cmath>

namespace {
QString levelText(double v) { return QString::number(v, 'g', 3); }
QString freqText(double mhz) { return QString::number(mhz, 'g', 10); }
bool sameF(double a, double b) { return std::abs(a - b) < 5e-7; }

// C# ListBoxes lined columns up with tab stops under heading labels; a QListWidget doesn't expand tabs,
// so the columns are padded with spaces to start under the heading labels instead.
int columnX(const QListWidget *list, const QLabel *heading, bool centre)
{
    const int x = heading->x() + (centre ? heading->width() / 2 - 3 : 0);
    return x - list->x() - list->frameWidth() - 3;
}

QString columns(const QListWidget *list, const QStringList &parts, const QList<int> &xs)
{
    const QFontMetrics fm(list->font());
    QString s = parts.value(0);
    for (int i = 1; i < parts.size(); ++i) {
        const int target = xs.value(i - 1);
        s += ' ';
        while (fm.horizontalAdvance(s + ' ') <= target)
            s += ' ';
        s += parts[i];
    }
    return s;
}

QLabel *headingFor(const QListWidget *list, const QString &startsWith)
{
    for (QLabel *l : list->parentWidget()->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly))
        if (l->text().startsWith(startsWith, Qt::CaseInsensitive) && l->y() < list->y())
            return l;
    return nullptr;
}

QString freqLine(const QListWidget *list, const FrequencyLevel &f)
{
    const QLabel *h = headingFor(list, "TX audio");
    const int x = h ? columnX(list, h, false) : 80;
    return columns(list, {freqText(f.mhz), levelText(f.level)}, {x});
}
} // namespace

double MainWindow::txLevelFor(double mhz) const
{
    for (const FrequencyLevel &f : m_freqLevels)
        if (sameF(f.mhz, mhz))
            return f.level;
    return 1.0;
}

void MainWindow::setupFreqAnt()
{
    fillAntennaList();

    // C#: the list starts with the standard frequencies at level 1.0 and takes levels from the database
    QVector<FrequencyLevel> list;
    for (int i = 0; i < ui->FreqlistBox->count(); ++i) {
        const QStringList p = ui->FreqlistBox->item(i)->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (!p.isEmpty())
            list << FrequencyLevel{p[0].trimmed().toDouble(), p.size() > 1 ? p.last().trimmed().toDouble() : 1.0};
    }
    for (const FrequencyLevel &db : m_store.loadFrequencies()) {
        bool found = false;
        for (FrequencyLevel &f : list)
            if (sameF(f.mhz, db.mhz)) {
                f.level = db.level;
                found = true;
            }
        if (!found)
            list << db;   // a frequency the user added
    }
    std::sort(list.begin(), list.end(), [](const FrequencyLevel &a, const FrequencyLevel &b) { return a.mhz < b.mhz; });
    m_freqLevels = list;
    ui->FreqlistBox->clear();
    for (const FrequencyLevel &f : std::as_const(m_freqLevels))
        ui->FreqlistBox->addItem(freqLine(ui->FreqlistBox, f));
    updateFrequencyboxes();

    auto *v = new QDoubleValidator(0.0, 300000.0, 6, ui->FtextBox);
    v->setNotation(QDoubleValidator::StandardNotation);
    v->setLocale(QLocale::c());   // always '.' (C# allowed digits and '.')
    ui->FtextBox->setValidator(v);
    ui->trackBar1->setRange(0, 100);
    ui->AntgroupBox->hide();
    ui->FreqgroupBox->hide();
}

// C#: the AntlistBox part of ReadAntennas: name padded to 40, then switch, switch 2, tuner
void MainWindow::fillAntennaList()
{
    ui->AntlistBox->clear();
    QList<int> xs;
    for (const char *h : {"Local", "Remote", "Tuner"}) {
        const QLabel *l = headingFor(ui->AntlistBox, h);
        xs << (l ? columnX(ui->AntlistBox, l, true) : 200 + 50 * xs.size());
    }
    for (const AntennaInfo &a : std::as_const(m_antennas))
        ui->AntlistBox->addItem(columns(ui->AntlistBox, {a.name.left(36), QString::number(a.sw), QString::number(a.sw2),
                                                         QString::number(a.tuner)}, xs));
}

// C#: updateFrequencyboxes - the TX test list and default frequency, plus the slot editor's list
void MainWindow::updateFrequencyboxes()
{
    QStringList fs;
    for (const FrequencyLevel &f : std::as_const(m_freqLevels))
        fs << freqText(f.mhz);
    {
        const QString keep = ui->FlistBox->currentItem() ? ui->FlistBox->currentItem()->text() : QString();
        const QSignalBlocker b(ui->FlistBox);
        ui->FlistBox->clear();
        ui->FlistBox->addItems(fs);
        const auto m = ui->FlistBox->findItems(keep, Qt::MatchExactly);
        if (!m.isEmpty())
            ui->FlistBox->setCurrentItem(m.first());
    }
    for (QComboBox *c : {ui->defaultFcomboBox, ui->FreqcomboBox}) {
        const double keep = c->currentText().toDouble();
        const QSignalBlocker b(c);
        c->clear();
        c->addItems(fs);
        for (int i = 0; i < c->count(); ++i)
            if (sameF(c->itemText(i).toDouble(), keep))
                c->setCurrentIndex(i);
    }
    if (ui->defaultFcomboBox->currentIndex() < 0)
        for (int i = 0; i < ui->defaultFcomboBox->count(); ++i)
            if (sameF(ui->defaultFcomboBox->itemText(i).toDouble(), m_cfg.defaultF))
                ui->defaultFcomboBox->setCurrentIndex(i);
}

// ---- frequencies --------------------------------------------------------------------------

void MainWindow::editF()
{
    const int row = ui->FreqlistBox->currentRow();
    if (row < 0 || row >= m_freqLevels.size()) {
        Msg::ok(this, "No frequency selected");
        return;
    }
    const FrequencyLevel f = m_freqLevels[row];
    const QString ft = freqText(f.mhz);
    // C#: 22m and 8m are receive only
    const bool rxOnly = ft.startsWith("40.") || ft.startsWith("13.");
    ui->trackBar1->setEnabled(!rxOnly);
    ui->RXonlylabel->setText(rxOnly ? "RX only on this band" : "-");
    ui->RXonlylabel->adjustSize();
    ui->FreqlistBox->setEnabled(false);
    ui->FtextBox->setText(ft);
    {
        const QSignalBlocker b(ui->trackBar1);
        ui->trackBar1->setValue(rxOnly ? 0 : int(std::lround(f.level * 100)));
    }
    ui->Levellabel->setText(levelText(rxOnly ? 0.0 : f.level));
    ui->Levellabel->adjustSize();
    m_newFreq = false;
    ui->FtextBox->setEnabled(false);   // C#: the frequency itself is not editable here
    ui->FreqgroupBox->show();
    ui->FBgroupBox->hide();
}

void MainWindow::closeFreqEditor()
{
    ui->FreqlistBox->setEnabled(true);
    ui->FreqgroupBox->hide();
    ui->FBgroupBox->show();
}

void MainWindow::editFbutton_Click() { editF(); }
void MainWindow::FreqlistBox_MouseDoubleClick(QListWidgetItem *) { editF(); }
void MainWindow::Fcancelbutton_Click() { closeFreqEditor(); }

void MainWindow::trackBar1_ValueChanged(int value)
{
    const double v = value / 100.0;
    ui->Levellabel->setText(levelText(v));
    ui->Levellabel->adjustSize();
    m_txVolume = float(v);   // C#: Volume - also changes a transmission in progress
    if (m_transmitting && m_txAudio)
        m_txAudio->setVolume(m_txVolume);
}

void MainWindow::FtextBox_TextChanged(const QString &) {}   // the validator does the checking

bool MainWindow::FtextBox_KeyPress(QKeyEvent *) { return false; }

// C#: saveFreq
void MainWindow::Fsavebutton_Click()
{
    bool ok = false;
    const double mhz = ui->FtextBox->text().trimmed().toDouble(&ok);
    if (!ok || mhz <= 0) {
        Msg::ok(this, "Invalid frequency");
        return;
    }
    if (m_newFreq)
        for (const FrequencyLevel &f : std::as_const(m_freqLevels))
            if (sameF(f.mhz, mhz)) {
                Msg::ok(this, "Frequency already exists");
                return;
            }
    const FrequencyLevel f{mhz, ui->trackBar1->value() / 100.0};
    if (!m_store.saveFrequency(f)) {
        Msg::ok(this, "Unable to save frequency settings: " + m_store.lastError());
        return;
    }
    m_txVolume = float(f.level);
    if (m_newFreq) {
        m_freqLevels << f;
        std::sort(m_freqLevels.begin(), m_freqLevels.end(),
                  [](const FrequencyLevel &a, const FrequencyLevel &b) { return a.mhz < b.mhz; });
    } else {
        for (FrequencyLevel &x : m_freqLevels)
            if (sameF(x.mhz, mhz))
                x.level = f.level;
    }
    const int keepRow = ui->FreqlistBox->currentRow();
    ui->FreqlistBox->clear();
    for (const FrequencyLevel &x : std::as_const(m_freqLevels))
        ui->FreqlistBox->addItem(freqLine(ui->FreqlistBox, x));
    ui->FreqlistBox->setCurrentRow(keepRow);
    closeFreqEditor();
    updateFrequencyboxes();
}

// C#: PopulateFbutton (hidden in the C# designer as well)
void MainWindow::PopulateFbutton_Click()
{
    if (!Msg::yesNo(this, "Reset frequency list to WSPR standard list (Y/N)?", "Reset freq list"))
        return;
    for (const FrequencyLevel &f : std::as_const(m_freqLevels))
        m_store.deleteFrequency(f.mhz);
    m_freqLevels.clear();
    bool allOk = true;
    for (int i = 0; i < ui->FstandardtlistBox->count(); ++i) {
        const FrequencyLevel f{ui->FstandardtlistBox->item(i)->text().toDouble(), 1.0};
        m_freqLevels << f;
        allOk &= m_store.saveFrequency(f);
    }
    ui->FreqlistBox->clear();
    for (const FrequencyLevel &x : std::as_const(m_freqLevels))
        ui->FreqlistBox->addItem(freqLine(ui->FreqlistBox, x));
    updateFrequencyboxes();
    Msg::ok(this, allOk ? "Database and list updated" : "List updated, some database writes failed: " + m_store.lastError(),
            "Reset complete");
}

// ---- antennas -----------------------------------------------------------------------------

void MainWindow::setDefaultSwitchesandTuners()
{
    ui->ShowSwlistBox->setCurrentRow(0);
    ui->ShowSwlistBox2->setCurrentRow(0);
    ui->ShowTulistBox->setCurrentRow(0);
    ui->AntPortlistBox->setCurrentRow(0);
    ui->AntPortlistBox2->setCurrentRow(0);
}

void MainWindow::addAnt()
{
    int next = 0;   // C# used the list length, which could re-use a number still in the database
    for (const AntennaInfo &a : std::as_const(m_antennas))
        next = qMax(next, a.antNo + 1);
    ui->antNolabel->setText(QString::number(next));
    ui->AntnametextBox->clear();
    ui->AntdesctextBox->clear();
    m_newAnt = true;
    m_oldAntName.clear();
    ui->AntgroupBox->show();
    ui->AgroupBox->hide();
    ui->AntlistBox->setEnabled(false);
    ui->AntnametextBox->setFocus();
}

void MainWindow::editAnt()
{
    const int sel = ui->AntlistBox->currentRow();
    if (sel < 0 || sel >= m_antennas.size()) {
        Msg::ok(this, "No antenna selected");
        return;
    }
    const AntennaInfo &a = m_antennas[sel];
    ui->antNolabel->setText(QString::number(a.antNo));
    ui->AntnametextBox->setText(a.name);
    m_oldAntName = a.name;
    ui->ShowSwlistBox->setCurrentRow(qBound(0, a.sw, ui->ShowSwlistBox->count() - 1));
    ui->ShowSwlistBox2->setCurrentRow(qBound(0, a.sw2, ui->ShowSwlistBox2->count() - 1));
    ui->ShowTulistBox->setCurrentRow(qBound(0, a.tuner, ui->ShowTulistBox->count() - 1));
    if (a.swPort >= 0 && a.swPort < ui->AntPortlistBox->count())
        ui->AntPortlistBox->setCurrentRow(a.swPort);   // C#: stored as the list index ("1" = 0)
    if (a.swPort2 >= 0 && a.swPort2 < ui->AntPortlistBox2->count())
        ui->AntPortlistBox2->setCurrentRow(a.swPort2);
    ShowSwlistBox_SelectedIndexChanged(ui->ShowSwlistBox->currentRow());
    ShowSwlistBox2_SelectedIndexChanged(ui->ShowSwlistBox2->currentRow());
    ui->AntdesctextBox->setPlainText(a.description);
    m_newAnt = false;
    ui->AntgroupBox->show();
    ui->AgroupBox->hide();
    ui->AntlistBox->setEnabled(false);
}

void MainWindow::delAnt()
{
    const int sel = ui->AntlistBox->currentRow();
    if (sel < 0 || sel >= m_antennas.size()) {
        Msg::ok(this, "No antenna selected");
        return;
    }
    const AntennaInfo a = m_antennas[sel];
    if (!Msg::yesNo(this, "Delete antenna: " + a.name + "...", "Delete Ant"))
        return;
    if (!m_store.deleteAntenna(a.antNo)) {
        Msg::ok(this, "Error deleting antenna: " + m_store.lastError());
        return;
    }
    m_antennas.remove(sel);
    delete ui->AntlistBox->takeItem(sel);
    for (QComboBox *c : {ui->DefaultAntcomboBox, ui->AntselcomboBox}) {
        const int i = c->findText(a.name);
        if (i >= 0)
            c->removeItem(i);
    }
}

void MainWindow::closeAntEditor()
{
    ui->AntgroupBox->hide();
    ui->AgroupBox->show();
    ui->AntlistBox->setEnabled(true);
}

bool MainWindow::saveAnt()
{
    const QString name = ui->AntnametextBox->text().trimmed();
    if (name.isEmpty()) {
        Msg::ok(this, "Antenna name is empty");
        return false;
    }
    if (m_newAnt)
        for (const AntennaInfo &x : std::as_const(m_antennas))
            if (x.name.trimmed().compare(name, Qt::CaseInsensitive) == 0) {
                Msg::ok(this, "Antenna already exists");
                return false;
            }
    AntennaInfo a;
    a.antNo = ui->antNolabel->text().toInt();
    a.name = name;
    a.description = ui->AntdesctextBox->toPlainText();
    a.sw = qMax(0, ui->ShowSwlistBox->currentRow());
    a.tuner = qMax(0, ui->ShowTulistBox->currentRow());
    a.swPort = qMax(0, ui->AntPortlistBox->currentRow());
    a.sw2 = qMax(0, ui->ShowSwlistBox2->currentRow());
    a.swPort2 = qMax(0, ui->AntPortlistBox2->currentRow());
    if (!m_store.saveAntenna(a)) {
        Msg::ok(this, "Unable to save antenna settings: " + m_store.lastError());
        return false;
    }
    if (m_newAnt) {
        m_antennas << a;
        ui->DefaultAntcomboBox->addItem(a.name);
        ui->AntselcomboBox->addItem(a.name);
    } else {
        const int sel = ui->AntlistBox->currentRow();
        if (sel >= 0 && sel < m_antennas.size())
            m_antennas[sel] = a;
        for (QComboBox *c : {ui->DefaultAntcomboBox, ui->AntselcomboBox}) {
            const int i = c->findText(m_oldAntName);
            if (i >= 0)
                c->setItemText(i, a.name);
        }
    }
    const int keep = ui->AntlistBox->currentRow();
    fillAntennaList();
    ui->AntlistBox->setCurrentRow(m_newAnt ? ui->AntlistBox->count() - 1 : keep);
    closeAntEditor();
    return true;
}

// C#: updateDefaultAnt - rebuild the default antenna list, keeping the selection
void MainWindow::updateDefaultAnt()
{
    const QString keep = ui->DefaultAntcomboBox->currentText();
    const QSignalBlocker b(ui->DefaultAntcomboBox);
    ui->DefaultAntcomboBox->clear();
    for (const AntennaInfo &a : std::as_const(m_antennas))
        ui->DefaultAntcomboBox->addItem(a.name);
    const int i = ui->DefaultAntcomboBox->findText(keep);
    ui->DefaultAntcomboBox->setCurrentIndex(i >= 0 ? i : (ui->DefaultAntcomboBox->count() ? 0 : -1));
}

void MainWindow::AddAbutton_Click()
{
    setDefaultSwitchesandTuners();
    addAnt();
}

void MainWindow::EditAbutton_Click()
{
    setDefaultSwitchesandTuners();
    editAnt();
}

void MainWindow::AntlistBox_MouseDoubleClick(QListWidgetItem *) { editAnt(); }
void MainWindow::DelAbutton_Click() { delAnt(); }
void MainWindow::CancelAbutton_Click() { closeAntEditor(); }

void MainWindow::SaveAbutton_Click()
{
    const bool wasNew = m_newAnt;
    const QString oldName = m_oldAntName;
    if (!saveAnt() || wasNew)
        return;
    updateDefaultAnt();
    const QString newName = ui->AntnametextBox->text().trimmed();
    if (oldName != newName
        && Msg::yesNo(this, "Update all slots from now with new name (Y/N)?", "Update slots?")) {
        if (!m_store.renameAntennaInSlots(oldName, newName, schedNow().date()))
            Msg::ok(this, "Error in some slots: " + m_store.lastError(), "Updating slots");
        else
            reloadCurrentHour();
    }
}

// Port lists only make sense with a switch (C#: ShowSwlistBox / ShowSwlistBox2 SelectedIndexChanged)
void MainWindow::ShowSwlistBox_SelectedIndexChanged(int row)
{
    const bool v = row > 0;
    ui->AntPortlistBox->setVisible(v);
    ui->AntPortlabel->setVisible(v);
}

void MainWindow::ShowSwlistBox2_SelectedIndexChanged(int row)
{
    const bool v = row > 0;
    ui->AntPortlistBox2->setVisible(v);
    ui->AntPortlabel2->setVisible(v);
}
