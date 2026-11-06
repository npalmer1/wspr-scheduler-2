// Slots tab: calendar / hour list / slot grid / slot editor.
// Ported from Form1.cs (changeDateTimes, currHour, EditSlot, SaveSlotbutton_Click, SaveSlot,
// SaveSlot_Sun, locateSlotMembersDT*, deletethisRow, DeleteSlot, selectRangtoDelete, FlushSlots, ...)
//
// Structure: SlotStore does all the SQL, planSlotTimes() works out repeat sets, SlotTableModel
// shows one hour. The handlers here only move values between the widgets and those three.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

#include "credentials.h"
#include "msg.h"
#include "repeatplanner.h"
#include "slottablemodel.h"

#include <QApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QHeaderView>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QSettings>
#include <QSignalBlocker>
#include <QTextCharFormat>
#include <QCalendarWidget>
#include <QLocale>
#include <cmath>

namespace {
const QString kDateFmt = QStringLiteral("yyyy-MM-dd");

QString slotKey(const QDate &d, const QTime &t)   // parent string format used by the C# app
{
    return d.toString(kDateFmt) + ' ' + t.toString("HH:mm");
}

void selectListText(QListWidget *list, const QString &text)
{
    const auto hits = list->findItems(text, Qt::MatchExactly);
    if (!hits.isEmpty())
        list->setCurrentItem(hits.first());
}
} // namespace

// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------

void MainWindow::setupSlotGrid()
{
    m_slotModel = new SlotTableModel(this);
    QTableView *v = ui->dataGridView1;
    v->setModel(m_slotModel);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    for (int c = 0; c < SlotTableModel::ColumnCount; ++c)
        v->setColumnWidth(c, SlotTableModel::defaultWidth(c));
    v->setColumnHidden(SlotTableModel::ColRptTime, true);   // C# hid column 13
    v->verticalHeader()->setDefaultSectionSize(20);
    v->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    v->verticalHeader()->setFixedWidth(20);
    v->horizontalHeader()->setHighlightSections(false);

    ui->OffsettextBox->setValidator(new QIntValidator(-20, 220, this));
    ui->greylistBox->setCurrentRow(0);
}

// C#: getUserandPassword. Password from ~/DBcredential (same file and encryption as the C# app);
// anything set in the app settings (db/host, db/port, db/user, db/password) overrides it.
DbSettings MainWindow::loadDbSettings() const
{
    DbSettings d;   // C# defaults: 127.0.0.1, admin / wspr
    if (const auto pw = Security::readDbPassword())
        d.password = *pw;
    QSettings s;
    d.host = s.value("db/host", d.host).toString();
    d.port = s.value("db/port", d.port).toInt();
    d.user = s.value("db/user", d.user).toString();
    d.password = s.value("db/password", d.password).toString();
    return d;
}

// C#: the slot-related part of Form1_Load
void MainWindow::Form1_Load()
{
    setWindowTitle("WSPR Scheduler          V." WSPRSKED_VERSION "     GNU GPLv3 License");   // as C#
    m_store.setSettings(loadDbSettings());
    {   // the database user and password, shown as *** (Show ticks to see it)
        const QSignalBlocker b(ui->PasstextBox);
        ui->Usernamelabel->setText(m_store.settings().user);
        ui->Usernamelabel->adjustSize();
        ui->label23->adjustSize();   // "WSPR database username:" - keep the name clear of it
        if (ui->Usernamelabel->x() < ui->label23->geometry().right() + 6)
            ui->Usernamelabel->move(ui->label23->geometry().right() + 6, ui->Usernamelabel->y());
        ui->PasstextBox->setEchoMode(QLineEdit::Password);
        ui->PasstextBox2->setEchoMode(QLineEdit::Password);
        ui->PasstextBox->setText(m_store.settings().password);
    }

    if (auto cfg = m_store.loadConfig())
        m_cfg = *cfg;
    applyConfigToUi();

    m_store.setSlotDbName(m_cfg.slotDb);
    if (m_store.slotDbReachable()) {
        m_store.ensureIndexes();
        loadAntennas();
        setDbStatus(true);
    } else {
        setDbStatus(false);
        Msg::timed(this, "Unable to contact database\n" + m_store.lastError(), "Database", 4000);
    }

    {
        const QSignalBlocker b1(ui->repeatcheckBox), b2(ui->repeatTimecheckBox), b3(ui->DaycheckBox),
            b4(ui->NightcheckBox), b5(ui->AllcheckBox);
        ui->repeatcheckBox->setChecked(false);
        ui->repeatTimecheckBox->setChecked(false);
        ui->DaycheckBox->setChecked(false);
        ui->NightcheckBox->setChecked(false);
        ui->AllcheckBox->setChecked(false);
    }
    ui->TXRXAntlabel->setText(m_cfg.defaultAnt);
    ui->TXRXAntlabel2->setText(m_cfg.defaultAnt);
    ui->trackSlotscheckBox->setChecked(true);
    LTcheckBox_CheckedChanged(ui->LTcheckBox->isChecked());
    updateClockLabels();
    currHour(false, false);
    setupFreqAnt();
    setupHardware();
    setupRig();
    setupReceiver();
    setupTransmit();
    setupSunAndClock();
    setupConfigTab();
    setupHardwareTests();
    setupOtherRigs();
    setupRemote();
    setupHelp();
}

void MainWindow::applyConfigToUi()
{
    ui->CalltextBox->setText(m_cfg.callsign);
    ui->baseCalltextBox->setText(m_cfg.baseCall);
    ui->LocatortextBox->setText(m_cfg.locator);
    ui->longcheckBox->setChecked(m_cfg.longLocator);
    ui->Type2checkBox->setChecked(m_cfg.allowType2);
    ui->asOnecheckBox->setChecked(m_cfg.oneMsg);
    ui->defaultFcomboBox->setCurrentText(QString::number(m_cfg.defaultF, 'g', 10));
    ui->defaultOfftextBox->setText(QString::number(m_cfg.defaultOffset));
    ui->defaultpwrcomboBox->setCurrentText(QString::number(m_cfg.defaultdBm));
    {
        const QSignalBlocker b(ui->LTcheckBox);
        ui->LTcheckBox->setChecked(m_cfg.localTime);
    }
    ui->dbbutton->setText(m_cfg.slotDb == "wspr_slots_test" ? "test" : "main");
    {
        const QSignalBlocker b(ui->stopRXcheckBox);
        ui->stopRXcheckBox->setChecked(m_cfg.stopRX);
        m_stopRX = m_cfg.stopRX;
    }
}

// C#: ReadAntennas (Slots-tab part)
void MainWindow::loadAntennas()
{
    m_antennas = m_store.loadAntennas();
    const QSignalBlocker b(ui->AntselcomboBox);
    ui->AntselcomboBox->clear();
    ui->DefaultAntcomboBox->clear();
    for (const AntennaInfo &a : std::as_const(m_antennas)) {
        ui->AntselcomboBox->addItem(a.name);
        ui->DefaultAntcomboBox->addItem(a.name);
    }
    ui->DefaultAntcomboBox->setCurrentText(m_cfg.defaultAnt);
}

void MainWindow::Form1_Shown()
{
    showReceiver();
    // Qt has no MySQL driver of its own: say so plainly rather than "server not running"
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        Msg::ok(this,
                "Qt's MySQL driver is missing, so the database can't be opened "
                "(the server itself may be fine).\n\n"
                "Put qsqlmysql.dll (built for this Qt version and compiler) in:\n"
                + QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/sqldrivers") +
                "\nand libmysql.dll, libcrypto-3-x64.dll, libssl-3-x64.dll in:\n"
                + QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) +
                "\n\nDrivers Qt can see: " + QSqlDatabase::drivers().join(", "),
                "MySQL driver not found");
        return;
    }
    if (!m_store.slotDbReachable())
        Msg::ok(this,
                "Cannot reach the MySQL server or the slot database.\n\n"
                "Server " + m_store.settings().host + ":" + QString::number(m_store.settings().port)
                + ", user " + m_store.settings().user + ", database " + m_store.slotDbName() + ".\n"
                "Check the server is running and the user/password (~/DBcredential, or the "
                "db/host, db/user, db/password settings).\n\nMySQL said: " + m_store.lastError(),
                "MySQL Not Running");
}

void MainWindow::setDbStatus(bool ok)
{
    m_dbOk = ok;
    ui->dblabel->setText(ok ? "Slot Database OK" : "Slot Database Error");
    ui->dblabel->setStyleSheet(QString("QLabel#dblabel { background-color: %1; }").arg(ok ? "lightblue" : "pink"));
    ui->dblabel->adjustSize();
    if (ok)
        m_showDbError = true;
}

// ---------------------------------------------------------------------------
// Time base and clock
// ---------------------------------------------------------------------------

// Grid time base: UTC unless "Use local time" is ticked
QDateTime MainWindow::schedNow() const
{
    return ui->LTcheckBox->isChecked() ? QDateTime::currentDateTime() : QDateTime::currentDateTimeUtc();
}

void MainWindow::updateClockLabels()
{
    const QDateTime now = schedNow();
    ui->Timelabel->setText(now.toString("HH:mm:ss"));
    ui->LTlabel->setText(QDateTime::currentDateTime().toString("HH:mm"));
    ui->Datelabel->setText(QLocale(QLocale::English).toString(now, "ddd-dd-MMM-yyyy"));   // C#: Mon-02-Nov-2026
    positionClockBar();
}

// C#: daytimer_Action - clock and slot tracking only. The rest of daytimer_Action
// (yearly sunrise refresh, solar data, rx form clock) comes with those features.
void MainWindow::daytimer_Tick()
{
    updateClockLabels();
    {
        const QDateTime utc = QDateTime::currentDateTimeUtc();
        const int sec = utc.time().second();
        if (sec == 2 || sec == 38)
            getRigF();   // C#: getRigF at seconds 2 and 38
        txTick(utc);
        clockAndSunTick(utc);
        receiverTick(utc);
    }

    const QTime t = schedNow().time();
    const int m = t.minute(), s = t.second();
    const bool nearTrigger = (m % 2 == 1 && s >= 49) || (m % 2 == 0 && s <= 4);
    ++m_idleSeconds;

    if (m % 2 == 0 && s < 6)
        m_slotMoved = false;   // new slot: allow one automatic move

    // After 90 s with no keyboard/mouse use, follow the current slot (C#: keypresses > 90)
    if (m_idleSeconds > 90 && s >= 6 && s <= 8 && !m_slotMoved && !ui->slotgroupBox->isVisible() && !nearTrigger) {
        m_slotMoved = true;
        if (ui->trackSlotscheckBox->isChecked())
            currHour(true, true);
        else
            m_idleSeconds = 0;
    }
}

void MainWindow::markActivity()
{
    m_idleSeconds = 0;
}

bool MainWindow::Form1_KeyDown_1(QKeyEvent *e)
{
    markActivity();
    (void)e;   // F1 is a shortcut now (setupHelp), so it works wherever the focus is
    return false;
}

void MainWindow::LTcheckBox_CheckedChanged(bool checked)
{
    ui->UTClabel->setText(checked ? "LT" : "UTC");
    ui->UTClabel2->setText(checked ? "LT" : "UTC");
    ui->LTlabel->setVisible(!checked);
    ui->LTlabel2->setVisible(!checked);
    positionClockBar();
    updateClockLabels();
}

// ---------------------------------------------------------------------------
// Showing an hour of slots
// ---------------------------------------------------------------------------

// C#: currHour. exactMin selects the slot running now (or the next one in its last 8 s).
// The C# version worked out the hour before adjusting the minute, so at hh:59:53 it jumped to hh:00
// instead of (hh+1):00, and its -1 minute adjustment made the grid look 2 minutes early.
void MainWindow::currHour(bool noChangeIfMatch, bool exactMin)
{
    QDateTime dt = schedNow();
    QTime target(dt.time().hour(), 0);
    if (exactMin) {
        const QTime t = dt.time();
        if (t.minute() % 2 == 1)
            dt = dt.addSecs(t.second() > 52 ? 60 : -60);   // next slot if it is about to start
        target = QTime(dt.time().hour(), dt.time().minute() - dt.time().minute() % 2);
    } else {
        target = QTime(dt.time().hour(), 0);
    }
    const QDate date = dt.date();

    if (noChangeIfMatch && m_slotModel->date() == date && m_slotModel->hour() == target.hour()
        && selectedSlotRow() == m_slotModel->rowForTime(target))
        return;

    {
        const QSignalBlocker b1(ui->monthCalendar1), b2(ui->timelistBox);
        ui->monthCalendar1->setSelectedDate(date);
        ui->timelistBox->setCurrentRow(target.hour());
    }
    if (date < m_calLeft || date >= m_calLeft.addMonths(2))
        m_calLeft = QDate(date.year(), date.month(), 1);
    syncCalendars();
    m_selectedDate = date;
    showHour(date, target.hour(), exactMin ? std::optional<QTime>(target) : std::nullopt);
}

// C#: changeDateTimes + findSlotRow + populateGridSlot (one query instead of 30)
void MainWindow::showHour(const QDate &date, int hour, std::optional<QTime> select)
{
    const auto hourSlots = m_store.loadHour(date, hour);
    if (hourSlots) {
        setDbStatus(true);
    } else {
        setDbStatus(false);
        if (m_showDbError) {
            Msg::timed(this, "Unable to read from database\n" + m_store.lastError(), "", 1500);
            m_showDbError = false;
        }
    }
    m_slotModel->setHour(date, hour, hourSlots.value_or(QMap<QTime, SlotRecord>{}));
    // dates can be wider than the C# widths with some fonts: grow, never shrink
    for (int c : {int(SlotTableModel::ColDate), int(SlotTableModel::ColEnd), int(SlotTableModel::ColAntenna)})
        ui->dataGridView1->setColumnWidth(c, qMax(SlotTableModel::defaultWidth(c), static_cast<QAbstractItemView *>(ui->dataGridView1)->sizeHintForColumn(c)));
    updateSunLabels(date, hour);

    if (select) {
        const int row = m_slotModel->rowForTime(*select);
        if (row >= 0) {
            ui->dataGridView1->selectRow(row);
            ui->dataGridView1->scrollTo(m_slotModel->index(row, SlotTableModel::ColTime),
                                        QAbstractItemView::PositionAtTop);
        }
    }
}

void MainWindow::reloadCurrentHour()
{
    const int row = selectedSlotRow();
    showHour(m_slotModel->date(), m_slotModel->hour(),
             row >= 0 ? std::optional<QTime>(m_slotModel->rowTime(row)) : std::nullopt);
}

int MainWindow::selectedSlotRow() const
{
    const auto rows = ui->dataGridView1->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

QString MainWindow::locator4() const
{
    return ui->LocatortextBox->text().trimmed().left(4).toUpper();
}

// Sunrise/sunset are stored as local times (C#: find_sunrise_sunset)
void MainWindow::updateSunLabels(const QDate &date, int hour)
{
    const auto sun = m_store.sunTimes(locator4(), date);
    if (!sun) {
        for (QLabel *l : {ui->localriselabel, ui->localsetlabel, ui->utcriselabel, ui->utcsetlabel})
            l->setText("N/A");
    } else {
        Q_UNUSED(hour)
        ui->localriselabel->setText(sun->rise.toString("HH:mm"));
        ui->localsetlabel->setText(sun->set.toString("HH:mm"));
        ui->utcriselabel->setText(QDateTime(date, sun->rise).toUTC().toString("HH:mm"));
        ui->utcsetlabel->setText(QDateTime(date, sun->set).toUTC().toString("HH:mm"));
    }
    for (QLabel *l : {ui->localriselabel, ui->localsetlabel, ui->utcriselabel, ui->utcsetlabel})
        l->adjustSize();
}

// Sun window for the repeat planner, in the grid's time base
std::optional<SunWindow> MainWindow::sunWindow(const QDate &date)
{
    const QString loc = locator4();
    if (loc != m_sunLocator) {
        m_sunCache = m_store.allSunTimes(loc);   // one query for the whole year
        m_sunLocator = loc;
    }
    const auto it = m_sunCache.constFind(date.toString("MM-dd"));
    if (it == m_sunCache.constEnd())
        return std::nullopt;
    if (ui->LTcheckBox->isChecked())
        return SunWindow{it->rise, it->set};
    return SunWindow{QDateTime(date, it->rise).toUTC().time(),
                     QDateTime(date, it->set).toUTC().time()};
}

void MainWindow::monthCalendar1_DateChanged()
{
    const QDate d = ui->monthCalendar1->selectedDate();
    if (d == m_selectedDate || m_rangeMode)
        return;
    m_selectedDate = d;
    showHour(d, qMax(0, ui->timelistBox->currentRow()), std::nullopt);
}

void MainWindow::timelistBox_SelectedIndexChanged(int row)
{
    if (row < 0)
        return;
    m_showDbError = true;
    showHour(m_selectedDate.isValid() ? m_selectedDate : ui->monthCalendar1->selectedDate(), row, std::nullopt);
}

void MainWindow::currHourbutton_Click()
{
    markActivity();
    m_showDbError = true;
    currHour(false, true);
}

// ---------------------------------------------------------------------------
// Grid clicks
// ---------------------------------------------------------------------------

void MainWindow::dataGridView1_CellMouseClick(const QModelIndex &index, Qt::MouseButton button)
{
    markActivity();
    if (!index.isValid())
        return;
    ui->dataGridView1->selectRow(index.row());
    if (button == Qt::RightButton)
        deleteThisRow();
}

void MainWindow::dataGridView1_CellMouseDoubleClick(const QModelIndex &index)
{
    markActivity();
    if (!index.isValid())
        return;
    ui->dataGridView1->selectRow(index.row());
    openSlotEditor(index.row());
}

// ---------------------------------------------------------------------------
// Slot editor (C#: slotSelected / EditSlot / findRepeatType)
// ---------------------------------------------------------------------------

void MainWindow::openSlotEditor(int row)
{
    if (ui->saveslotlabel->isVisible()) {
        Msg::timed(this, "Please wait...", "Saving slots", 1500);
        return;
    }
    m_editRow = row;
    m_editDate = m_slotModel->date();
    m_editTime = m_slotModel->rowTime(row);
    m_editOriginal = m_slotModel->slot(row);
    // the slot after this one (second half of a type 2/3 pair), possibly in the next hour
    const QDateTime next = QDateTime(m_editDate, m_editTime).addSecs(120);
    m_editNextOriginal = (row + 1 < SlotTableModel::kRows) ? m_slotModel->slot(row + 1)
                                                            : m_store.find(next.date(), next.time());

    if (m_editDate < schedNow().date())
        Msg::timed(this, "Note: editing a date before today", "", 2000);

    m_loadingEditor = true;
    ui->slotlabel->hide();
    ui->greygroupBox->hide();
    ui->datetimelabel->setText(m_editDate.toString(kDateFmt) + "  " + m_editTime.toString("HH:mm"));
    ui->datetimelabel->adjustSize();
    if (m_editOriginal)
        fillEditorFromSlot(*m_editOriginal);
    else
        fillEditorDefaults();
    m_loadingEditor = false;
    updateRepeatUi();

    m_repeatStatus = ui->repeatcheckBox->isChecked();
    ui->slotgroupBox->show();
    ui->slotgroupBox->raise();
}

void MainWindow::selectFrequency(double mhz)
{
    for (int i = 0; i < ui->FreqcomboBox->count(); ++i) {
        if (std::abs(ui->FreqcomboBox->itemText(i).toDouble() - mhz) < 1e-6) {
            ui->FreqcomboBox->setCurrentIndex(i);
            return;
        }
    }
}

void MainWindow::fillEditorDefaults()
{
    const QSignalBlocker b1(ui->repeatcheckBox), b2(ui->repeatTimecheckBox), b3(ui->DaycheckBox),
        b4(ui->NightcheckBox), b5(ui->AllcheckBox), b6(ui->ActivecheckBox);
    ui->editslotcheckBox->setChecked(false);   // creating a new slot
    takeDefaultsFromUi();   // the TX Configuration values as they are now, saved or not
    selectFrequency(m_cfg.defaultF);
    ui->OffsettextBox->setText(QString::number(m_cfg.defaultOffset));
    ui->dBmcomboBox->setCurrentText(QString::number(m_cfg.defaultdBm));
    ui->PowertextBox->setText(dBmToWatts(m_cfg.defaultdBm));
    ui->AntselcomboBox->setCurrentText(m_cfg.defaultAnt);
    applyAntenna(ui->AntselcomboBox->currentText());
    ui->dateEnd->setDate(m_editDate);
    ui->timeEnd->setTime(QTime(23, 0));
    ui->ActivecheckBox->setChecked(true);
    ui->repeatcheckBox->setChecked(false);
    ui->repeatTimecheckBox->setChecked(false);
    ui->DaycheckBox->setChecked(false);
    ui->NightcheckBox->setChecked(false);
    ui->AllcheckBox->setChecked(false);
    m_origRptType = 0;
}

void MainWindow::fillEditorFromSlot(const SlotRecord &s)
{
    const QSignalBlocker b1(ui->repeatcheckBox), b2(ui->repeatTimecheckBox), b3(ui->DaycheckBox),
        b4(ui->NightcheckBox), b5(ui->AllcheckBox), b6(ui->ActivecheckBox);
    ui->editslotcheckBox->setChecked(true);    // editing an existing slot
    selectFrequency(s.freq);
    ui->OffsettextBox->setText(QString::number(s.offset));
    ui->dBmcomboBox->setCurrentText(QString::number(s.powerdBm));
    ui->PowertextBox->setText(dBmToWatts(s.powerdBm));
    ui->AntselcomboBox->setCurrentText(s.antenna);
    applyAntenna(s.antenna);                   // switch/port shown from the antenna table, as C#
    ui->selTunertextBox->setText(QString::number(s.tuner));
    ui->msgTlabel->setText(s.msgType == 2 ? "Message type 2" : "Message type 1");
    ui->dateEnd->setDate(s.endDate.isValid() ? s.endDate : m_editDate);
    ui->timeEnd->setTime(s.rptType == RptUntilTime && s.endTime.isValid() ? s.endTime : QTime(23, 59));
    ui->ActivecheckBox->setChecked(s.active);
    ui->repeatcheckBox->setChecked(s.repeating);

    const int rpt = s.repeating ? s.rptType : 0;   // C#: findRepeatType
    m_origRptType = rpt;
    ui->repeatTimecheckBox->setChecked(rpt == RptUntilTime);
    ui->DaycheckBox->setChecked(rpt == RptDay);
    ui->NightcheckBox->setChecked(rpt == RptNight);
    ui->AllcheckBox->setChecked(rpt == RptAllDay);
    selectListText(ui->greylistBox, QString::number(s.greyOffset));
}

// C#: AntselcomboBox_SelectedIndexChanged. Ports are shown starting from 1 (0 = no switch).
void MainWindow::applyAntenna(const QString &name)
{
    for (const AntennaInfo &a : std::as_const(m_antennas)) {
        if (a.name.trimmed() != name.trimmed())
            continue;
        ui->selSwitchtextBox->setText(QString::number(a.sw));
        ui->selSwitchtextBox2->setText(QString::number(a.sw2));
        ui->selTunertextBox->setText(QString::number(a.tuner));
        ui->selSwPorttextBox->setText(QString::number(a.sw == 0 ? 0 : a.swPort + 1));
        ui->selSwPorttextBox2->setText(QString::number(a.sw2 == 0 ? 0 : a.swPort2 + 1));
        return;
    }
}

// C#: repeatcheckBox_CheckedChanged visibility rules (+ checkPeriod)
void MainWindow::updateRepeatUi()
{
    const bool rpt = ui->repeatcheckBox->isChecked();
    ui->dateEnd->setEnabled(rpt);
    ui->timeEnd->setVisible(rpt);
    ui->timeEnd->setEnabled(rpt && ui->repeatTimecheckBox->isChecked());
    for (QWidget *w : {static_cast<QWidget *>(ui->repeatTimecheckBox), static_cast<QWidget *>(ui->DaycheckBox),
                       static_cast<QWidget *>(ui->NightcheckBox), static_cast<QWidget *>(ui->AllcheckBox)})
        w->setVisible(rpt);
    ui->greygroupBox->setVisible(rpt && (ui->DaycheckBox->isChecked() || ui->NightcheckBox->isChecked()));
    if (ui->greygroupBox->isVisible() && !ui->greylistBox->currentItem())
        ui->greylistBox->setCurrentRow(0);
    checkPeriod();
}

// C#: checkPeriod - warn that long ranges are slow (much less so now, but kept as a hint)
void MainWindow::checkPeriod()
{
    const int days = m_editDate.isValid() ? int(m_editDate.daysTo(ui->dateEnd->date())) : 0;
    int t = 0;
    if (ui->repeatcheckBox->isChecked()) {
        if (ui->repeatTimecheckBox->isChecked() && days > 120) t = 120;
        else if ((ui->DaycheckBox->isChecked() || ui->NightcheckBox->isChecked()) && days > 60) t = 60;
        else if (ui->AllcheckBox->isChecked() && days > 80) t = 80;
        else if (days > 180) t = 180;
    }
    ui->rangelabel->setVisible(t > 0);
    if (t > 0) {
        ui->rangelabel->setText(QString("Note: saving may be slower if the date range is more than %1 days").arg(t));
        ui->rangelabel->adjustSize();
    }
}

int MainWindow::rptTypeFromUi() const   // C#: findRptType (0 when not repeating)
{
    if (!ui->repeatcheckBox->isChecked()) return RptNone;
    if (ui->repeatTimecheckBox->isChecked()) return RptUntilTime;
    if (ui->DaycheckBox->isChecked()) return RptDay;
    if (ui->NightcheckBox->isChecked()) return RptNight;
    if (ui->AllcheckBox->isChecked()) return RptAllDay;
    return RptNone;
}

void MainWindow::repeatcheckBox_CheckedChanged(bool checked)
{
    if (!m_loadingEditor && !checked && m_repeatStatus) {
        if (!Msg::yesNo(this, "Only modify this slot (Y/N)?", "Modify slots")) {
            const QSignalBlocker b(ui->repeatcheckBox);
            ui->repeatcheckBox->setChecked(true);
        }
    }
    updateRepeatUi();
}

// Repeat options are mutually exclusive (C#: repeatTime/Day/Night/AllcheckBox_CheckedChanged)
void MainWindow::exclusiveRepeat(QCheckBox *keep)
{
    for (QCheckBox *c : {ui->repeatTimecheckBox, ui->DaycheckBox, ui->NightcheckBox, ui->AllcheckBox}) {
        if (c != keep) {
            const QSignalBlocker b(c);
            c->setChecked(false);
        }
    }
}

void MainWindow::repeatTimecheckBox_CheckedChanged(bool checked)
{
    if (checked) exclusiveRepeat(ui->repeatTimecheckBox);
    updateRepeatUi();
}

void MainWindow::DaycheckBox_CheckedChanged(bool checked)
{
    if (checked) exclusiveRepeat(ui->DaycheckBox);
    updateRepeatUi();
}

void MainWindow::NightcheckBox_CheckedChanged(bool checked)
{
    if (checked) exclusiveRepeat(ui->NightcheckBox);
    updateRepeatUi();
}

void MainWindow::AllcheckBox_CheckedChanged(bool checked)
{
    if (checked) exclusiveRepeat(ui->AllcheckBox);
    updateRepeatUi();
}

static bool isRxOnlyBand(const QString &freq)   // 22 m (13.5539) and 8 m (40.68)
{
    return freq.startsWith("40") || freq.startsWith("13");
}

void MainWindow::ActivecheckBox_CheckedChanged(bool checked)
{
    if (checked && isRxOnlyBand(ui->FreqcomboBox->currentText())) {
        Msg::timed(this, "22m and 8m RX only slots", "22m and 8m bands", 1500);
        const QSignalBlocker b(ui->ActivecheckBox);
        ui->ActivecheckBox->setChecked(false);
    }
}

void MainWindow::dBmcomboBox_SelectedIndexChanged(int)
{
    ui->PowertextBox->setText(dBmToWatts(ui->dBmcomboBox->currentText().toInt()));
}

void MainWindow::AntselcomboBox_SelectedIndexChanged(int)
{
    applyAntenna(ui->AntselcomboBox->currentText());
}

void MainWindow::dateEnd_ValueChanged(QDate)
{
    checkPeriod();
}

void MainWindow::timeEnd_ValueChanged(QTime)
{
    // C# popped up a message on every change; the end time is now checked when saving
}

void MainWindow::OffsettextBox_TextChanged(const QString &)
{
}

bool MainWindow::OffsettextBox_KeyPress(QKeyEvent *)
{
    return false;   // the QIntValidator set in setupSlotGrid() limits input to -20..220
}

void MainWindow::CancelSlotbutton_Click()
{
    closeSlotEditor();
}

void MainWindow::closeSlotEditor()
{
    ui->slotgroupBox->hide();
    ui->greygroupBox->hide();
    m_repeatStatus = false;
    m_editRow = -1;
    ui->dataGridView1->setFocus();
}

// ---------------------------------------------------------------------------
// Saving (C#: SaveSlotbutton_Click / SaveSlot / SaveSlot_Sun / SaveSlotData)
// ---------------------------------------------------------------------------

// C#: validateSlot
bool MainWindow::validateSlot()
{
    bool ok = false;
    const int off = ui->OffsettextBox->text().toInt(&ok);
    if (ui->FreqcomboBox->currentText().isEmpty() || ui->dBmcomboBox->currentText().isEmpty() || !ok) {
        Msg::ok(this, "Missing data");
        return false;
    }
    if (off < -20 || off > 220) {
        Msg::ok(this, "Error: offset range 0-200 Hz");
        return false;
    }
    const int rpt = rptTypeFromUi();
    if (ui->repeatcheckBox->isChecked() && rpt != RptDay && rpt != RptNight && rpt != RptAllDay) {
        if (ui->dateEnd->date() < m_editDate) {
            Msg::ok(this, "End date must be >= slot start date");
            return false;
        }
        if (rpt == RptUntilTime && ui->timeEnd->time() < m_editTime) {
            Msg::ok(this, "End time must be >= slot time");
            return false;
        }
    }
    return true;
}

// C#: checkCall - which WSPR message type the callsign/locator needs (0 = invalid)
int MainWindow::checkCall(const QString &call, QString locator)
{
    if (!ui->longcheckBox->isChecked())
        locator = locator.left(4);
    if (locator.length() > 4)
        return 3;
    if (call.trimmed().isEmpty())
        return 0;
    bool hasDigit = false;
    for (const QChar c : call)
        hasDigit = hasDigit || c.isDigit();
    if (!hasDigit) {
        Msg::ok(this, "Call should contain at least one number");
        return 0;
    }
    if (call.length() > 10) {
        Msg::ok(this, "Callsign over 10 characters");
        return 0;
    }
    if (call.length() > 6)
        return ui->asOnecheckBox->isChecked() ? 3 : 2;
    return 1;
}

// C#: checkNextSlot - type 2 needs the following slot too
bool MainWindow::checkNextSlot(int row)
{
    if (row >= 0 && row < SlotTableModel::kRows - 1) {
        if (m_slotModel->slot(row + 1)) {
            Msg::ok(this, "Type 2 message - adjacent slot occupied", "Next slot occupied");
            return Msg::yesNo(this, "Overwrite adjacent slot(s) (y/n)?", "Next slot");
        }
        return true;
    }
    if (m_editNextOriginal) {
        Msg::ok(this, "Type 2 message - adjacent slot (next hour) occupied", "Next slot occupied");
        return Msg::yesNo(this, "Overwrite adjacent slot anyway (y/n)?", "Next slot");
    }
    return true;
}

// Builds the slot from the editor and writes it, plus its repeats, in one transaction
bool MainWindow::saveSlotSet(const QDate &date, const QTime &time, int slotNo, int msgT, bool thisSlotOnly,
                             const QString &parent)
{
    SlotRecord r;
    r.freq = std::round(ui->FreqcomboBox->currentText().toDouble() * 10000.0) / 10000.0;
    r.offset = ui->OffsettextBox->text().toInt();
    r.powerdBm = ui->dBmcomboBox->currentText().toInt();
    r.powerW = dBmToWatts(r.powerdBm).toDouble();   // C# SaveSlot_Sun used ToInt32 here and failed below 1 W
    r.antenna = ui->AntselcomboBox->currentIndex() >= 0 ? ui->AntselcomboBox->currentText() : QString();
    r.tuner = ui->selTunertextBox->text().toInt();
    r.sw = ui->selSwitchtextBox->text().toInt();
    r.sw2 = ui->selSwitchtextBox2->text().toInt();
    r.swPort = ui->selSwPorttextBox->text().toInt() + 1;    // C# stores shown value + 1 ("zeroise"); kept so
    r.swPort2 = ui->selSwPorttextBox2->text().toInt() + 1;  // both apps read the same rows the same way
    r.active = ui->ActivecheckBox->isChecked();
    r.repeating = ui->repeatcheckBox->isChecked();
    r.rptType = rptTypeFromUi();
    r.rptTime = r.rptType == RptUntilTime;
    r.endDate = r.repeating ? ui->dateEnd->date() : date;
    r.endTime = r.rptType == RptUntilTime ? ui->timeEnd->time() : time;
    r.slotNo = slotNo;
    r.msgType = msgT;
    if (r.rptType == RptDay || r.rptType == RptNight) {
        const QListWidgetItem *it = ui->greylistBox->currentItem();
        r.greyOffset = it ? it->text().toInt() : 0;
        if (r.greyOffset == 127)
            r.greyOffset = 120;
    }
    r.parent = parent;

    RepeatRequest req;
    req.startDate = date;
    req.slotTime = time;
    req.repeat = r.repeating;
    req.rptType = r.rptType;
    req.endDate = r.endDate;
    req.endTime = r.endTime;
    req.greyOffset = r.greyOffset;
    req.thisSlotOnly = thisSlotOnly;

    QString err;
    const SlotTimes times = planSlotTimes(req, [this](const QDate &d) { return sunWindow(d); }, &err);
    if (times.isEmpty()) {
        Msg::ok(this, "Error: " + err, "Save slot");
        return false;
    }
    QVector<SlotRecord> rows;
    rows.reserve(times.size());
    for (const auto &[d, t] : times) {
        SlotRecord x = r;
        x.date = d;
        x.time = t;
        rows << x;
    }
    if (!m_store.upsertMany(rows)) {
        Msg::ok(this, "Error saving slots\n" + m_store.lastError(), "Save slot");
        setDbStatus(false);
        return false;
    }
    return true;
}

// Parent of a repeat set: keep the original when editing, otherwise this slot's own date/time
QString MainWindow::parentFor(const QDate &d, const QTime &t, const std::optional<SlotRecord> &original,
                              int rptType) const
{
    if (ui->editslotcheckBox->isChecked() && original && !original->parent.isEmpty())
        return original->parent;
    if (rptType == RptAllDay)
        return d.toString(kDateFmt) + "00:00";   // C# format for 24 h sets (no space)
    return slotKey(d, t);
}

// Deletes the slot at d/t, or its whole repeat set
void MainWindow::deleteSlotAt(const QDate &d, const QTime &t, bool all, const QString &parent)
{
    const bool ok = (all && !parent.isEmpty()) ? m_store.deleteByParent(parent) : m_store.deleteOne(d, t);
    if (!ok) {
        Msg::ok(this, "Error deleting slot\n" + m_store.lastError());
        setDbStatus(false);
    }
}

void MainWindow::SaveSlotbutton_Click()
{
    if (m_editRow < 0 || !validateSlot())
        return;

    int msgT = 1;
    if (ui->Type2checkBox->isChecked())
        msgT = checkCall(ui->CalltextBox->text().trimmed(), ui->LocatortextBox->text().trimmed());
    if (!ui->ActivecheckBox->isChecked() || msgT == 0)
        msgT = 1;
    if (msgT == 2 || msgT == 3) {
        if (Msg::yesNo(this, "Save as message type 2/3 (Y/N)?", "Message type 2/3"))
            ui->msgTlabel->setText("Message type 2");
        else
            msgT = 1;   // C# re-ran checkCall when writing and saved type 2 anyway
    }

    const bool editing = ui->editslotcheckBox->isChecked();
    const int newR = rptTypeFromUi();
    const bool pairOld = m_editOriginal && (m_editOriginal->msgType == 2 || m_editOriginal->msgType == 3)
                         && !ui->asOnecheckBox->isChecked();
    const QDateTime nextOld = QDateTime(m_editDate, m_editTime).addSecs(120);
    bool thisSlotOnly;
    if (ui->repeatcheckBox->isChecked() && editing) {
        const bool all = Msg::yesNo(this, "Update all repeating slots (Y/N)?", "Repeating slots");
        if (newR != m_origRptType) {   // repeat type changed: remove the old set first
            deleteSlotAt(m_editDate, m_editTime, all, m_editOriginal ? m_editOriginal->parent : QString());
            if (pairOld && m_editNextOriginal)
                deleteSlotAt(nextOld.date(), nextOld.time(), all, m_editNextOriginal->parent);
        }
        thisSlotOnly = !all;
    } else {
        thisSlotOnly = !ui->repeatcheckBox->isChecked();
    }

    if (isRxOnlyBand(ui->FreqcomboBox->currentText()) && ui->ActivecheckBox->isChecked()) {
        Msg::timed(this, "22 and 8m RX only", "22m and 8m", 2500);
        const QSignalBlocker b(ui->ActivecheckBox);
        ui->ActivecheckBox->setChecked(false);
    }

    ui->saveslotlabel->show();
    ui->slotlabel->setText("Saving ... please wait");
    ui->slotlabel->adjustSize();
    ui->slotlabel->show();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    qApp->processEvents();

    bool ok;
    if ((msgT == 2 || msgT == 3) && !ui->asOnecheckBox->isChecked() && checkNextSlot(m_editRow)) {
        // Type 2/3 pair: this slot and the one 2 minutes later (which may be in the next hour/day;
        // C# wrapped hh:58 round to hh:00 of the same hour)
        const QDateTime second = QDateTime(m_editDate, m_editTime).addSecs(120);
        ok = saveSlotSet(m_editDate, m_editTime, 1, msgT, thisSlotOnly,
                         parentFor(m_editDate, m_editTime, m_editOriginal, newR));
        ok = ok && saveSlotSet(second.date(), second.time(), 2, msgT, thisSlotOnly,
                               parentFor(second.date(), second.time(), m_editNextOriginal, newR));
    } else {
        ok = saveSlotSet(m_editDate, m_editTime, 1, msgT, thisSlotOnly,
                         parentFor(m_editDate, m_editTime, m_editOriginal, newR));
    }

    QApplication::restoreOverrideCursor();
    ui->saveslotlabel->hide();
    ui->slotlabel->hide();
    if (ok)
        closeSlotEditor();
    reloadCurrentHour();
}

// ---------------------------------------------------------------------------
// Deleting (C#: deletethisRow / DeleteSlot / selectRangtoDelete / FlushSlots)
// ---------------------------------------------------------------------------

void MainWindow::deleteThisRow()
{
    const int row = selectedSlotRow();
    if (row < 0 || !m_slotModel->slot(row))
        return;
    const SlotRecord s = *m_slotModel->slot(row);
    if (!Msg::yesNo(this, "Delete this slot (Y/N)?", "Delete Slot"))
        return;
    const bool all = s.repeating && Msg::yesNo(this, "Delete all repeating slots too (Y/N)?", "Delete Repeat Slots");

    deleteSlotAt(s.date, s.time, all, s.parent);
    if ((s.msgType == 2 || s.msgType == 3) && !ui->asOnecheckBox->isChecked()) {
        const QDateTime next = QDateTime(s.date, s.time).addSecs(120);
        if (const auto n = m_store.find(next.date(), next.time()))
            deleteSlotAt(n->date, n->time, all, n->parent);
    }
    reloadCurrentHour();
}

// C#: selectRangtoDelete. MonthCalendar could drag-select a range; QCalendarWidget can't,
// so in range mode the first click sets the start date and the second the end date.
void MainWindow::deleteAllSlotsbutton_Click()
{
    if (!m_rangeMode) {
        m_rangeMode = true;
        m_rangeStart = m_rangeEnd = QDate();
        ui->deleteAllSlotsbutton->setText("Select date range from");
        showRangeSelection();
        Msg::timed(this, "Click the first and last dates on the calendar,\nthen press the button again.",
                   "Delete slots", 3000);
        return;
    }
    if (!m_rangeStart.isValid() || !m_rangeEnd.isValid()) {
        if (Msg::yesNo(this, "Invalid date range - cancel (Y/N)?", "Invalid date range"))
            endRangeMode();
        return;
    }
    const QString d1 = m_rangeStart.toString(kDateFmt), d2 = m_rangeEnd.toString(kDateFmt);
    if (Msg::yesNo(this, "Delete all slots between selected dates (y/n)?", "Delete selected days")) {
        if (m_store.deleteBetween(m_rangeStart, m_rangeEnd))
            Msg::ok(this, "Deleted slots from: " + d1 + " to: " + d2, "Success");
        else
            Msg::ok(this, "Error deleting slots\n" + m_store.lastError());
    }
    endRangeMode();
    reloadCurrentHour();
}

void MainWindow::monthCalendar1_DateSelected(QDate date)
{
    if (!m_rangeMode)
        return;
    if (!m_rangeStart.isValid() || m_rangeEnd.isValid()) {
        m_rangeStart = date;
        m_rangeEnd = QDate();
    } else {
        m_rangeEnd = date;
        if (m_rangeEnd < m_rangeStart)
            std::swap(m_rangeStart, m_rangeEnd);
    }
    showRangeSelection();
}

void MainWindow::showRangeSelection()
{
    for (QCalendarWidget *c : {ui->monthCalendar1, m_cal2})
        if (c) c->setDateTextFormat(QDate(), QTextCharFormat());   // clear
    QTextCharFormat f;
    f.setBackground(QColor(255, 200, 120));
    if (m_rangeStart.isValid()) {
        const QDate last = m_rangeEnd.isValid() ? m_rangeEnd : m_rangeStart;
        for (QDate d = m_rangeStart; d <= last; d = d.addDays(1))
            for (QCalendarWidget *c : {ui->monthCalendar1, m_cal2})
                if (c) c->setDateTextFormat(d, f);
    }
    syncCalendars();   // put the grey of the other months' days back
    ui->selectedStartDatelabel->setText(m_rangeStart.isValid() ? m_rangeStart.toString(kDateFmt) : "date1");
    ui->selectedEndDatelabel->setText(m_rangeEnd.isValid() ? m_rangeEnd.toString(kDateFmt) : "date2");
    ui->selectedStartDatelabel->adjustSize();
    ui->selectedEndDatelabel->adjustSize();
}

void MainWindow::endRangeMode()
{
    m_rangeMode = false;
    m_rangeStart = m_rangeEnd = QDate();
    showRangeSelection();
    ui->deleteAllSlotsbutton->setText("Delete all slots from");
}

void MainWindow::flsuhSlotbutton_Click()
{
    const QDate cutoff = QDateTime::currentDateTimeUtc().date().addMonths(-6);
    if (!Msg::yesNo(this, "Delete all slots older than 6 months (y/n)?", "Flush old slots"))
        return;
    if (!Msg::yesNo(this, "Flush slots before " + cutoff.toString(kDateFmt) + " (y/n)?", "Confirmation"))
        return;
    if (m_store.deleteBefore(cutoff))
        Msg::ok(this, "Deleted slots before " + cutoff.toString(kDateFmt), "Success");
    else
        Msg::ok(this, "Error deleting slots\n" + m_store.lastError());
    reloadCurrentHour();
}

// C#: dbbutton_Click - switch between the main and test slot databases
void MainWindow::dbbutton_Click()
{
    if (!m_store.slotDbReachable()) {
        Msg::ok(this, "Database connection error", "Database Error");
        return;
    }
    const bool toTest = ui->dbbutton->text() == "main";
    if (!Msg::yesNo(this, QString("Switch slot database to \"%1\" (y/n)?").arg(toTest ? "test" : "main"),
                    "Database switch"))
        return;
    const QString name = toTest ? "wspr_slots_test" : "wspr_slots";
    if (toTest && !m_store.databaseExists(name) && !m_store.createSlotDatabase(name)) {
        Msg::ok(this, "Unable to create slot database\n" + m_store.lastError(), "Database Error");
        return;
    }
    m_store.setSlotDbName(name);
    m_store.ensureIndexes();
    m_cfg.slotDb = name;
    m_store.saveSlotDbName(m_cfg.configId, name);
    ui->dbbutton->setText(toTest ? "test" : "main");
    currHour(false, true);
}
