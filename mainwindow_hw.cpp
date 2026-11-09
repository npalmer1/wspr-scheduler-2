// Switches&Tuners tab: antenna switches, tuners and filters - name, connection (serial port + settings,
// or TCP/UDP IP + port), type and number of antenna ports; and the filter in use.
// (C#: ReadHardware, SaveHardware/UpdateHardware, editButtonAction, editHW, loadHWtoGroupBox,
// SaveSw/SaveTu/SaveFi, HwSavebutton/HwCancelbutton, setHwBoxes, FilistBox_MouseClick.)
//
// Differences from the C# version:
//  - one bound INSERT ... ON DUPLICATE KEY UPDATE (C# fell back to an UPDATE with the values pasted into
//    the SQL, so a name with an apostrophe could not be saved);
//  - choosing a switch type no longer overwrites the port count with another switch's (C# used the type's
//    row number to index the switch list);
//  - the selected filter is saved at once (C# only saved it with "Save Settings" or on exit);
//  - a saved COM port that isn't plugged in is kept in the list instead of being lost;
//  - the tuner and filter Edit buttons and lists are re-enabled after Cancel/Save (C# only re-enabled
//    the switch/tuner lists, so the filter list stayed greyed out after editing a filter).
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "msg.h"

#include <QIntValidator>
#include <QListWidget>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>

QListWidget *MainWindow::hwList(const QString &table) const
{
    if (table == "switches")
        return ui->SwlistBox;
    if (table == "tuners")
        return ui->TulistBox;
    return ui->FilistBox;
}

QVector<HardwareInfo> &MainWindow::hwItems(const QString &table)
{
    if (table == "switches")
        return m_sw;
    if (table == "tuners")
        return m_tu;
    return m_fil;
}

void MainWindow::setupHardware()
{
    // drop-down lists (C#: DropDownList style)
    for (QComboBox *c : {ui->HwProtocolcomboBox, ui->HwPortcomboBox, ui->HwBaudcomboBox, ui->HwDatacomboBox,
                         ui->HwParitycomboBox, ui->HwStopcomboBox, ui->HwFlowcomboBox})
        c->setEditable(false);
    for (const char *t : {"switches", "tuners", "filters"})
        loadHardwareList(t);
    m_selectedFilter = qBound(0, m_cfg.selectedFilter, ui->FilistBox->count() - 1);
    if (ui->FilistBox->count())
        ui->selFiltertextBox->setText(ui->FilistBox->item(m_selectedFilter)->text());
    ui->HwPorttextBox->setValidator(new QIntValidator(1, 65535, this));
    ui->HwIPtextBox->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9.]{0,15}"), this));
    ui->HwAntportstextBox->setValidator(new QIntValidator(1, 16, this));
    ui->HwtextBox->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9 ]*"), this));   // C#: letters/digits
    ui->HwgroupBox->hide();
    hideHwBoxes();
}

// C#: ReadHardware. The list keeps its "0  No switch", "1  switch1"... rows; saved ones get their names.
void MainWindow::loadHardwareList(const QString &table)
{
    QListWidget *list = hwList(table);
    QVector<HardwareInfo> &items = hwItems(table);
    items.clear();
    for (int i = 0; i < list->count(); ++i) {
        HardwareInfo blank;
        blank.id = i;
        items << blank;
    }
    const auto rows = m_store.loadHardware(table);
    if (!rows) {
        Msg::timed(this, "Unable to load " + table, "", 1000);
        return;
    }
    for (const HardwareInfo &h : *rows) {
        if (h.name.isEmpty() || h.id < 0 || h.id >= list->count())
            continue;
        items[h.id] = h;
        const QString text = QString::number(h.id) + "\t" + h.name;
        list->item(h.id)->setText(text);
        if (table == "switches") {
            if (h.id < ui->ShowSwlistBox->count())
                ui->ShowSwlistBox->item(h.id)->setText(text);
            if (h.id < ui->ShowSwlistBox2->count())
                ui->ShowSwlistBox2->item(h.id)->setText(text);
        } else if (table == "tuners" && h.id < ui->ShowTulistBox->count()) {
            ui->ShowTulistBox->item(h.id)->setText(text);
        }
    }
}

void MainWindow::editButtonAction(const QString &hwtype)
{
    // C#: setBoxDefaults
    ui->HwDatacomboBox->setCurrentText("8");
    ui->HwParitycomboBox->setCurrentIndex(0);
    ui->HwStopcomboBox->setCurrentIndex(0);
    ui->HwBaudcomboBox->setCurrentText("9600");
    ui->HwFlowcomboBox->setCurrentIndex(0);

    const QString table = hwtype == "switch" ? "switches" : hwtype == "tuner" ? "tuners" : "filters";
    const int sel = hwList(table)->currentRow();
    ui->HwAntportstextBox->setText(hwtype == "tuner" ? "1" : "4");
    if (sel < 0) {
        Msg::ok(this, "No " + hwtype + " selected");
        return;
    }
    if (sel == 0) {
        Msg::ok(this, "Cannot edit \"No " + hwtype + "\"");
        return;
    }
    if (!Msg::yesNo(this, "Edit " + hwtype + " settings (Y/N)?", "Edit " + hwtype))
        return;
    ui->HwgroupBox->setTitle(hwtype == "switch" ? "Switch" : hwtype == "tuner" ? "Tuner" : "Filter");
    editHW(table);
}

void MainWindow::editHW(const QString &table)
{
    m_hwTable = table;
    QListWidget *list = hwList(table);
    const int sel = list->currentRow();
    const HardwareInfo h = hwItems(table).value(sel);

    // type list for this kind of hardware (C#: showChannelBox)
    ui->HwSwTypegroupBox->setVisible(table == "switches");
    ui->HwTuTypegroupBox->setVisible(table == "tuners");
    ui->HwFiTypegroupBox->setVisible(table == "filters");
    QListWidget *types = table == "switches" ? ui->HwSwTypelistBox
                         : table == "tuners" ? ui->HwTuTypelistBox : ui->HwFiTypelistBox;
    types->setCurrentRow(-1);
    for (int i = 0; i < types->count(); ++i)
        if (types->item(i)->text() == h.type)
            types->setCurrentRow(i);
    if (table == "switches" && h.channels > 0 && !h.type.isEmpty())
        ui->HwAntportstextBox->setText(QString::number(h.channels));

    ui->editSwbutton->hide();   // C#: hideEditButtons
    ui->editTubutton->hide();
    ui->editFibutton->hide();
    list->setEnabled(false);
    const QStringList parts = list->currentItem()->text().split('\t');
    ui->Hwnolabel->setText(parts.value(0).trimmed());
    ui->Hwnolabel->adjustSize();
    ui->HwtextBox->setText(h.name.isEmpty() ? parts.value(1).trimmed() : h.name);

    // C#: loadHWtoGroupBox
    getComports();
    const QSignalBlocker b(ui->HwProtocolcomboBox);
    if (h.protocol.isEmpty()) {
        ui->HwProtocolcomboBox->setCurrentIndex(-1);
        ui->HwIPtextBox->clear();
        ui->HwPorttextBox->clear();
        hideHwBoxes();
    } else {
        ui->HwProtocolcomboBox->setCurrentText(h.protocol);
        if (h.protocol == "Serial") {
            setHwBoxes(true);
            ui->HwBaudcomboBox->setCurrentText(h.baud);
            if (!h.port.isEmpty() && ui->HwPortcomboBox->findText(h.port) < 0)
                ui->HwPortcomboBox->addItem(h.port);
            ui->HwPortcomboBox->setCurrentText(h.port);
            const QStringList s = h.serial.split(',');
            ui->HwDatacomboBox->setCurrentText(s.value(0, "8"));
            ui->HwParitycomboBox->setCurrentText(s.value(1, "none"));
            ui->HwStopcomboBox->setCurrentText(s.value(2, "1"));
            ui->HwFlowcomboBox->setCurrentText(s.value(3, "none"));
        } else {
            setHwBoxes(false);
            ui->HwIPtextBox->setText(h.ip);
            ui->HwPorttextBox->setText(h.port);
        }
    }
    ui->HwgroupBox->show();
    ui->HwtextBox->setFocus();
}

void MainWindow::placeLabelBeside(QLabel *label, const QWidget *box)
{
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    const int w = qMax(label->width(), label->fontMetrics().horizontalAdvance(label->text()) + 2);
    label->setGeometry(box->x() - 5 - w, box->y() + (box->height() - label->height()) / 2, w, label->height());
}

void MainWindow::setHwBoxes(bool serial)
{
    ui->HwPortcomboBox->setVisible(serial);
    ui->HwBaudcomboBox->setVisible(serial);
    ui->HwPorttextBox->setVisible(!serial);
    ui->HwIPtextBox->setVisible(!serial);
    ui->BPlabel->setText(serial ? "Baud:" : "IP:");
    placeLabelBeside(ui->BPlabel, serial ? static_cast<QWidget *>(ui->HwBaudcomboBox) : ui->HwIPtextBox);
    placeLabelBeside(ui->HwPortlabel, serial ? static_cast<QWidget *>(ui->HwPortcomboBox) : ui->HwPorttextBox);
    ui->BPlabel->setVisible(true);
    ui->HwPortlabel->setVisible(true);
    ui->HwSerialgroupBox->setVisible(serial);
}

void MainWindow::hideHwBoxes()
{
    for (QWidget *w : {static_cast<QWidget *>(ui->HwPortcomboBox), static_cast<QWidget *>(ui->HwPorttextBox),
                       static_cast<QWidget *>(ui->BPlabel), static_cast<QWidget *>(ui->HwBaudcomboBox),
                       static_cast<QWidget *>(ui->HwIPtextBox), static_cast<QWidget *>(ui->HwPortlabel),
                       static_cast<QWidget *>(ui->HwSerialgroupBox)})
        w->hide();
}

void MainWindow::closeHwEditor()
{
    hideHwBoxes();
    ui->HwgroupBox->hide();
    for (QListWidget *l : {ui->SwlistBox, ui->TulistBox, ui->FilistBox})
        l->setEnabled(true);
    ui->editSwbutton->show();
    ui->editTubutton->show();
    ui->editFibutton->show();
}

bool MainWindow::checkIP(const QString &ip)
{
    const QStringList p = ip.trimmed().split('.');
    if (p.size() != 4)
        return false;
    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        const int d = p[i].toInt(&ok);
        if (!ok || d < (i == 0 ? 1 : 0) || d > (i == 0 ? 254 : 255))
            return false;
    }
    return true;
}

void MainWindow::HwProtocolcomboBox_SelectedIndexChanged(int index)
{
    if (index < 0)
        return;
    setHwBoxes(index == 0);   // 0 = Serial, else TCP/UDP
    if (index == 0)
        getComports();
}

// C#: set the port count from the selected switch type - but it indexed the switch list with the
// type's row, so it showed another switch's count. Kept as typed here.
void MainWindow::HwSwTypelistBox_SelectedIndexChanged(int) {}

void MainWindow::HwSavebutton_Click()
{
    const int proto = ui->HwProtocolcomboBox->currentIndex();
    if (proto < 0) {
        Msg::ok(this, "No protocol selected");
        return;
    }
    const bool serial = ui->HwProtocolcomboBox->currentText() == "Serial";
    if (serial) {
        if (ui->HwPortcomboBox->currentIndex() < 0 || ui->HwBaudcomboBox->currentIndex() < 0) {
            Msg::ok(this, "Invalid port or baud rate");
            return;
        }
    } else {
        bool portOk = false;
        ui->HwPorttextBox->text().toInt(&portOk);
        if (!checkIP(ui->HwIPtextBox->text()) || !portOk) {
            Msg::ok(this, "Invalid IP or port");
            return;
        }
    }
    const QString kind = ui->HwgroupBox->title();   // "Switch", "Tuner", "Filter"
    QListWidget *types = m_hwTable == "switches" ? ui->HwSwTypelistBox
                         : m_hwTable == "tuners" ? ui->HwTuTypelistBox : ui->HwFiTypelistBox;
    if (m_hwTable == "switches") {
        const int ch = ui->HwAntportstextBox->text().toInt();
        if (ch <= 0) {
            Msg::ok(this, "Error: No channels on this switch", "Switch channels 1-16");
            return;
        }
    }
    if (types->currentRow() < 0) {
        Msg::ok(this, "You must select and highlight a " + kind.toLower() + " type", "No " + kind.toLower() + " type");
        return;
    }
    if (ui->HwtextBox->text().trimmed().isEmpty()) {
        Msg::ok(this, kind + " name is empty");
        return;
    }

    HardwareInfo h;
    h.id = ui->Hwnolabel->text().toInt();
    h.name = ui->HwtextBox->text().trimmed();
    h.protocol = ui->HwProtocolcomboBox->currentText();
    if (serial) {
        h.port = ui->HwPortcomboBox->currentText();
        h.baud = ui->HwBaudcomboBox->currentText();
        h.serial = ui->HwDatacomboBox->currentText() + "," + ui->HwParitycomboBox->currentText() + ","
                   + ui->HwStopcomboBox->currentText() + "," + ui->HwFlowcomboBox->currentText();
    } else {
        h.port = ui->HwPorttextBox->text().trimmed();
        h.ip = ui->HwIPtextBox->text().trimmed();
    }
    h.type = types->currentItem()->text().split('\t').value(0);
    h.channels = m_hwTable == "switches" ? ui->HwAntportstextBox->text().toInt() : (m_hwTable == "tuners" ? 1 : 4);
    if (m_hwTable == "filters")
        h.channels = qMax(1, ui->HwAntportstextBox->text().toInt());

    if (!m_store.saveHardware(m_hwTable, h)) {
        Msg::ok(this, "Unable to save " + m_hwTable + " settings: " + m_store.lastError());
        return;
    }
    QVector<HardwareInfo> &items = hwItems(m_hwTable);
    if (h.id >= 0 && h.id < items.size())
        items[h.id] = h;
    const QString text = QString::number(h.id) + "\t" + h.name;
    QListWidget *list = hwList(m_hwTable);
    if (h.id < list->count())
        list->item(h.id)->setText(text);
    if (m_hwTable == "switches") {
        for (QListWidget *l : {ui->ShowSwlistBox, ui->ShowSwlistBox2})
            if (h.id < l->count())
                l->item(h.id)->setText(text);
    } else if (m_hwTable == "tuners" && h.id < ui->ShowTulistBox->count()) {
        ui->ShowTulistBox->item(h.id)->setText(text);
    } else if (m_hwTable == "filters" && h.id == m_selectedFilter) {
        ui->selFiltertextBox->setText(text);
    }
    closeHwEditor();
    Msg::timed(this, "Saved", "", 500);
}

void MainWindow::HwCancelbutton_Click() { closeHwEditor(); }

void MainWindow::editSwbutton_Click() { editButtonAction("switch"); }
void MainWindow::editTubutton_Click() { editButtonAction("tuner"); }
void MainWindow::editFibutton_Click() { editButtonAction("filter"); }
void MainWindow::SwlistBox_MouseDoubleClick(QListWidgetItem *) { editButtonAction("switch"); }
void MainWindow::TulistBox_MouseDoubleClick(QListWidgetItem *) { editButtonAction("tuner"); }
void MainWindow::FilistBox_MouseDoubleClick(QListWidgetItem *) { editButtonAction("filter"); }

// C#: FilistBox_MouseClick - choose the filter in use
void MainWindow::FilistBox_MouseClick(QListWidgetItem *item)
{
    if (!item || !ui->FilistBox->isEnabled())
        return;
    const int i = ui->FilistBox->row(item);
    if (i == m_selectedFilter)
        return;
    if (!Msg::yesNo(this, "Select this as current filter (Y/N)?", "Select Filter"))
        return;
    m_selectedFilter = i;
    ui->selFiltertextBox->setText(item->text());
    if (!m_store.saveSelectedFilter(m_cfg.configId, i))
        Msg::timed(this, "Filter choice not saved: " + m_store.lastError(), "", 2500);
    else
        m_cfg.selectedFilter = i;
}

// Key filters: validators set in setupHardware do the C# KeyPress checks (digits for the port,
// digits and dots for the IP, letters and digits for the name)
bool MainWindow::HwPorttextBox_KeyPress(QKeyEvent *) { return false; }
bool MainWindow::HwIPtextBox_KeyPress(QKeyEvent *) { return false; }
bool MainWindow::HwtextBox_KeyPress(QKeyEvent *) { return false; }
