#include "rxform.h"
#include "ui_rxform.h"

#include "msg.h"
#include "rxcycle.h"
#include "rxtablemodel.h"
#include "spectrumform.h"
#include "wsprnetupload.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHeaderView>
#include <QIntValidator>
#include <QRegularExpressionValidator>
#include <QSortFilterProxyModel>
#include <QTimeZone>

RxForm::RxForm(QWidget *parent)
    : QWidget(parent, Qt::Window), ui(std::make_unique<Ui::RXForm>()), m_cycle(new RxCycle(this)),
      m_upload(new WsprnetUpload(this)), m_model(new RxTableModel(this)), m_proxy(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);
    // the status label never widens the window (long texts are cut short, see setLabel)
    ui->statuslabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ui->statuslabel->setMinimumWidth(fontMetrics().horizontalAdvance("idle/not receiving") + 8);
    ui->statuslabel->setMaximumWidth(fontMetrics().horizontalAdvance("idle/not receiving") * 2);
    m_spectrum = std::make_unique<SpectrumForm>();
    // C#: FormBorderStyle.Sizable with minimise/maximise, but no close button (CS_NOCLOSE)
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(215, 228, 242));   // SystemColors.GradientInactiveCaption
    setPalette(pal);
    setAutoFillBackground(true);

    // grid
    m_model->setMaxRows(m_maxRows);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(RxTableModel::SortRole);
    ui->dataGridView1->setModel(m_proxy);
    ui->dataGridView1->sortByColumn(RxTableModel::DateTime, Qt::DescendingOrder);
    const int widths[] = {110, 110, 100, 50, 50, 50, 55, 75, 75, 75, 60, 100, 100};
    for (int c = 0; c < RxTableModel::ColumnCount; ++c)
        ui->dataGridView1->setColumnWidth(c, widths[c]);
    ui->dataGridView1->verticalHeader()->setDefaultSectionSize(20);
    ui->dataGridView1->verticalHeader()->setMinimumWidth(18);
    ui->dataGridView1->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->dataGridView1->verticalHeader(), &QHeaderView::customContextMenuRequested, this,
            &RxForm::rowHeaderContextMenu);

    // filters
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (QDateTimeEdit *e : {ui->dateTimePicker1, ui->dateTimePicker2}) {   // filter times are UTC
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        e->setTimeZone(QTimeZone::utc());
#else
        e->setTimeSpec(Qt::UTC);
#endif
    }
    ui->dateTimePicker1->setDateTime(now.addDays(-1));
    ui->dateTimePicker2->setDateTime(now);
    ui->callFiltertextBox->setValidator(
        new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9./*<>-]*"), this));
    connect(ui->callFiltertextBox, &QLineEdit::textEdited, this, [this](const QString &t) {
        const int pos = ui->callFiltertextBox->cursorPosition();
        ui->callFiltertextBox->setText(t.toUpper());   // C#: KeyChar = ToUpper
        ui->callFiltertextBox->setCursorPosition(pos);
    });
    updateDistanceLimits();
    ui->OSDlistBox->setCurrentIndex(0);
    ui->bandlistBox->setCurrentIndex(0);
    ui->cwssblistBox->setCurrentText("100");
    m_model->setCompareWatts(100);

    connect(ui->button1, &QPushButton::clicked, this, &RxForm::button1_Click);
    connect(ui->filterbutton, &QPushButton::clicked, this, &RxForm::filterbutton_Click);
    connect(ui->Clearbutton, &QPushButton::clicked, this, &RxForm::Clearbutton_Click);
    connect(ui->OSDlistBox, &QComboBox::currentIndexChanged, this, &RxForm::OSDlistBox_SelectedIndexChanged);
    connect(ui->DeepcheckBox, &QCheckBox::clicked, this, &RxForm::DeepcheckBox_Clicked);
    connect(ui->QuickcheckBox, &QCheckBox::clicked, this, &RxForm::QuickcheckBox_Clicked);
    connect(ui->uploadcheckBox, &QCheckBox::clicked, this, [this] { saveConfig(); });
    connect(ui->kmcheckBox, &QCheckBox::toggled, this, &RxForm::kmcheckBox_CheckedChanged);
    connect(ui->stopRXcheckBox, &QCheckBox::toggled, this, &RxForm::stopRXcheckBox_CheckedChanged);
    connect(ui->cwssblistBox, &QComboBox::currentTextChanged, this, &RxForm::cwssblistBox_SelectedValueChanged);
    connect(ui->Spectrumbutton, &QPushButton::clicked, this, &RxForm::Spectrumbutton_Click);
    connect(ui->DFromtextBox, &QLineEdit::returnPressed, this, &RxForm::filterbutton_Click);
    connect(ui->DTotextBox, &QLineEdit::returnPressed, this, &RxForm::filterbutton_Click);
    connect(ui->callFiltertextBox, &QLineEdit::returnPressed, this, &RxForm::filterbutton_Click);

    // receive cycle
    connect(m_cycle, &RxCycle::statusChanged, this, &RxForm::setLabel);
    connect(m_cycle, &RxCycle::decoded, this, &RxForm::onDecoded);
    connect(m_cycle, &RxCycle::recordingStarted, this, [this](const QString &wav, int) {
        m_spectrum->setWavPath(wav, Rx::cycleStart(QDateTime::currentDateTimeUtc()));   // C#: Spectrumform.wavPath
    });
    connect(m_cycle, &RxCycle::problem, this, [this](const QString &m) {
        Rx::debugLog(m);
        Msg::timed(this, m, "WSPR receive", 2000);
    });
}

RxForm::~RxForm() = default;

void RxForm::setHeader(const QString &call, const DbSettings &db, const QString &locator, const QString &audioIn,
                       const QString &wsprdPath, const QString &version)
{
    setWindowTitle("Transmissions received by this station");
    m_version = "WS-" + version;
    m_upload->setUserAgent(m_version);
    changeHeader(call, db, locator, audioIn, wsprdPath);
    m_ready = true;
    readConfig();
    m_cycle->deleteRecordings();   // C# deleted temp1..3.wav from the wsprd folder (not where it recorded)
}

void RxForm::changeHeader(const QString &call, const DbSettings &db, const QString &locator, const QString &audioIn,
                          const QString &wsprdPath)
{
    m_call = call.trimmed();
    ui->ownlabel->setText(m_call);
    m_store.setSettings(db);
    m_loc = locator.trimmed();
    ui->myloclabel->setText(m_loc);
    m_cycle->setReporter(m_call, m_loc);
    setWsprdPath(wsprdPath);
    setAudioDevice(audioIn);
}

void RxForm::setFrequency(const QString &mhz)
{
    m_cycle->setDial(mhz);
    m_spectrum->setFrequency(mhz);
    ui->RXFlabel->setText(mhz);
    ui->myloclabel->setText(m_loc);
}

void RxForm::setTime(const QString &time, const QString &lt)
{
    ui->Timelabel->setText(time.startsWith(lt) ? time + " UTC" : time + " UTC    (" + lt + " LT)");
}

// Status line. Long texts (errors with the database's or wsprd's own message) used to widen the whole
// window: the label now keeps its width, shows a short text and pops the full message up for a few
// seconds (the same message at most once every 10 minutes); the full text is also its tooltip.
void RxForm::setLabel(const QString &status)
{
    const bool error = status.contains("error", Qt::CaseInsensitive);
    QString shown = status;
    if (error)
        shown = status.startsWith("Database", Qt::CaseInsensitive) ? QString("database error") : QString("RX error");
    else if (status.size() > 30)
        shown = status.left(28) + QChar(0x2026);
    ui->statuslabel->setText(shown);
    ui->statuslabel->setToolTip(status);
    if (!error)
        return;
    Rx::debugLog(status);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (status == m_lastPopup && m_lastPopupAt.isValid() && m_lastPopupAt.secsTo(now) < 600)
        return;
    m_lastPopup = status;
    m_lastPopupAt = now;
    Msg::timed(this, status.left(300), "Receive", 5000);
}

void RxForm::updateLabels(const QString &locator)
{
    m_loc = locator.trimmed();
    ui->myloclabel->setText(m_loc);
    m_cycle->setReporter(m_call, m_loc);
}

bool RxForm::startReceive()
{
    if (ui->stopRXcheckBox->isChecked())
        return false;
    RxCycle::Options o;
    o.deep = ui->DeepcheckBox->isChecked();
    o.quick = ui->QuickcheckBox->isChecked();
    o.osd = qMax(0, ui->OSDlistBox->currentIndex());
    m_cycle->setOptions(o);
    if (m_cycle->dial().isEmpty()) {
        setLabel("no RX frequency");
        return false;
    }
    return m_cycle->start();
}

void RxForm::setBlockDecodes(bool block) { m_cycle->setBlockDecodes(block); }
void RxForm::setGain(float gain) { m_cycle->setGain(gain); }
void RxForm::setWsprdPath(const QString &dir) { m_cycle->setWsprdDir(dir); }

bool RxForm::setAudioDevice(const QString &name)
{
    const bool ok = m_cycle->setDevice(name);
    if (!ok)
        Rx::debugLog("audio input \"" + name + "\" not found, using " + m_cycle->deviceName());
    return ok;
}

bool RxForm::useDeep() const { return ui->DeepcheckBox->isChecked(); }
int RxForm::osd() const { return qMax(0, ui->OSDlistBox->currentIndex()); }
bool RxForm::receivingStopped() const { return ui->stopRXcheckBox->isChecked(); }
void RxForm::clearLog() { Rx::clearDebugLog(); }

void RxForm::closeEvent(QCloseEvent *e)
{
    if (m_allowClose)
        e->accept();
    else
        e->ignore();   // C#: CS_NOCLOSE - the main window hides/shows this one
}

// ---- config -------------------------------------------------------------------------------

void RxForm::readConfig()
{
    const auto c = m_store.readConfig();
    if (!c)
        return;
    m_loadingConfig = true;
    ui->DeepcheckBox->setChecked(c->deep);
    ui->QuickcheckBox->setChecked(c->quick);
    ui->OSDlistBox->setCurrentIndex(c->osd);
    ui->uploadcheckBox->setChecked(c->upload);
    m_loadingConfig = false;
}

void RxForm::saveConfig()
{
    if (m_loadingConfig || !m_ready)
        return;
    RxConfig c;
    c.deep = ui->DeepcheckBox->isChecked();
    c.quick = ui->QuickcheckBox->isChecked();
    c.osd = qMax(0, ui->OSDlistBox->currentIndex());
    c.upload = ui->uploadcheckBox->isChecked();
    if (!m_store.saveConfig(c))
        Rx::debugLog("rxconfig not saved: " + m_store.lastError());
}

// ---- decodes ------------------------------------------------------------------------------

void RxForm::onDecoded(const Rx::DecodeResult &result, const QString &dial, const QDateTime &)
{
    if (ui->stopRXcheckBox->isChecked())   // C#: SaveReceived returns when stopped
        return;
    for (const RxSpot &s : result.spots) {
        if (!m_store.insert(s))
            setLabel("Database error - decode not saved: " + m_store.lastError());
        m_model->prepend(s);
        if (s.isNil() || m_stopUrl)
            continue;
        if (!ui->uploadcheckBox->isChecked()) {
            Rx::debugLog("upload skipped (unchecked) " + s.txSign);
            continue;
        }
        if (!Rx::shouldUpload(s, m_call, ui->owncheckBox->isChecked()))
            continue;
        m_upload->post(Rx::wsprnetForm(s, m_version, m_call, m_loc, dial), s.txSign);
    }
}

void RxForm::showRows(const std::optional<QVector<RxSpot>> &rows)
{
    if (!rows) {
        setLabel("Database error: " + m_store.lastError());
        return;
    }
    m_model->setRows(*rows);
    ui->dataGridView1->sortByColumn(RxTableModel::DateTime, Qt::DescendingOrder);
}

void RxForm::button1_Click()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto rows = m_store.latest(m_maxRows);
    QApplication::restoreOverrideCursor();
    showRows(rows);
}

void RxForm::Clearbutton_Click() { button1_Click(); }   // C#: show_results (all rows, no filter)

std::optional<int> RxForm::distanceKm(const QString &text) const
{
    bool ok = false;
    const int v = text.trimmed().toInt(&ok);
    if (!ok)
        return std::nullopt;
    return ui->kmcheckBox->isChecked() ? v : Rx::milesToKm(v);
}

void RxForm::filterbutton_Click()
{
    RxFilter f;
    f.useDates = ui->datecheckBox->isChecked();
    f.from = ui->dateTimePicker1->dateTime();
    f.to = ui->dateTimePicker2->dateTime();
    f.bandIndex = ui->bandlistBox->currentIndex();
    QString call = ui->callFiltertextBox->text();
    if (call.contains('*'))
        ui->callFiltertextBox->setText(call.remove('*'));   // as C#
    f.call = call;
    f.minKm = distanceKm(ui->DFromtextBox->text());
    f.maxKm = distanceKm(ui->DTotextBox->text());
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto rows = m_store.filtered(f, m_maxRows);
    QApplication::restoreOverrideCursor();
    showRows(rows);
}

// ---- small handlers -----------------------------------------------------------------------

void RxForm::OSDlistBox_SelectedIndexChanged(int index)
{
    QString warn;
    if (index > 3)
        warn = " (very slow)";
    else if (index > 1)
        warn = " (slow)";
    ui->OSDlabel->setText("OSD " + ui->OSDlistBox->itemText(qMax(0, index)) + warn);
    saveConfig();
}

void RxForm::DeepcheckBox_Clicked(bool checked)
{
    if (checked)
        ui->QuickcheckBox->setChecked(false);   // C#: only when the user ticks it (Focused)
    saveConfig();
}

void RxForm::QuickcheckBox_Clicked(bool checked)
{
    if (checked)
        ui->DeepcheckBox->setChecked(false);
    saveConfig();
}

void RxForm::updateDistanceLimits()
{
    const bool km = ui->kmcheckBox->isChecked();
    // C#: from 0-20000 km, to 0-20035 km (half the earth), converted when showing miles
    const int maxFrom = km ? 20000 : Rx::kmToMiles(20000);
    const int maxTo = km ? 20035 : Rx::kmToMiles(20035);
    ui->DFromtextBox->setValidator(new QIntValidator(0, maxFrom, ui->DFromtextBox));
    ui->DTotextBox->setValidator(new QIntValidator(0, maxTo, ui->DTotextBox));
    ui->Ulabel->setText(km ? "km" : "mls");
    ui->Dlabel->setText(km ? "0 - 20,035" : "0 - 12,453");
}

void RxForm::kmcheckBox_CheckedChanged(bool) { updateDistanceLimits(); }

void RxForm::stopRXcheckBox_CheckedChanged(bool checked)
{
    ui->RXofflabel->setText(checked ? "RX STOPPED" : "RX Running");
}

void RxForm::cwssblistBox_SelectedValueChanged(const QString &text)
{
    bool ok = false;
    const int w = text.toInt(&ok);
    m_model->setCompareWatts(ok ? w : 100);
}

void RxForm::Spectrumbutton_Click()
{
    m_spectrum->show();
    m_spectrum->raise();
    m_spectrum->activateWindow();
}

void RxForm::setAllowClose(bool allow)
{
    m_allowClose = allow;
    m_spectrum->setAllowClose(allow);
    if (allow)
        m_spectrum->close();
}

void RxForm::rowHeaderContextMenu(const QPoint &pos)
{
    if (!ui->allowDeletecheckBox->isChecked())
        return;
    const int viewRow = ui->dataGridView1->verticalHeader()->logicalIndexAt(pos);
    if (viewRow < 0)
        return;
    ui->dataGridView1->selectRow(viewRow);
    const int row = m_proxy->mapToSource(m_proxy->index(viewRow, 0)).row();
    if (row < 0)
        return;
    const RxSpot s = m_model->spot(row);
    const QString what = s.datetime.toString("yyyy-MM-dd HH:mm") + "  " + s.txSign + "  "
                         + QString::number(s.frequency, 'f', 6);
    if (!Msg::yesNo(this, "Delete this row (Y/N)?\n" + what, "Delete row"))
        return;
    if (!m_store.remove(s.datetime, s.txSign, s.frequency)) {
        Msg::ok(this, "Error deleting row: " + m_store.lastError());
        return;
    }
    m_model->removeAt(row);
}
