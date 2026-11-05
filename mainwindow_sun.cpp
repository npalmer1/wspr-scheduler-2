// Sunrise/sunset table, PC clock check and the Gaussian filter alpha (TX Configuration tab).
// C#: updateSunriseSunset / SaveSunRiseSunset (button, at start-up when the table is short, every 30 days),
// SyncSystemClockWithNTP (sync clock button, daily), AlphacomboBox_SelectedValueChanged.
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "maidenhead.h"
#include "msg.h"
#include "ntpcheck.h"
#include "rxspot.h"
#include "suntimes.h"
#include "slottablemodel.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QThread>
#include <QTimeZone>
#include <memory>

void MainWindow::setupSunAndClock()
{
    // Gaussian filter alpha: chosen from the list (C# swallowed typing), saved as soon as it changes
    ui->AlphacomboBox->setEditable(false);
    {
        const QSignalBlocker b(ui->AlphacomboBox);
        const QString a = QString::number(m_cfg.alpha, 'g', 4);
        if (ui->AlphacomboBox->findText(a) < 0)
            ui->AlphacomboBox->addItem(a);
        ui->AlphacomboBox->setCurrentIndex(ui->AlphacomboBox->findText(a));
    }

    if (!m_sunFetch) {
        m_sunFetch = new SunFetcher(this);
        const QString testUrl = qEnvironmentVariable("WSPRSKED_SUN_URL");   // tests only
        if (!testUrl.isEmpty())
            m_sunFetch->setBaseUrl(testUrl);
        connect(m_sunFetch, &SunFetcher::finished, this, &MainWindow::sunTimesFetched);
    }
    // C#: at start-up, rebuild the table when it has too few days for this locator
    const int have = m_store.allSunTimes(locator4()).size();
    if (have < 360 && !m_stopInternet) {
        ui->sunlabel->setText(have == 0 ? "No sunrise/sunset times - building table - please wait"
                                        : QString("Sunrise/set times only for %1 days - updating").arg(have));
        updateSunTimes(false);
    } else {
        ui->sunlabel->setText(QString("Sunrise/sunset times correct for this location.  %1").arg(Sun::kAttribution));
    }
    ui->sunlabel->adjustSize();
}

// C#: updateSunriseSunset. One request for the whole of this year, in this PC's time zone.
void MainWindow::updateSunTimes(bool announce)
{
    if (m_sunFetch->busy())
        return;
    const auto ll = Maidenhead::toLatLng(ui->LocatortextBox->text().trimmed());
    if (!ll) {
        if (announce)
            Msg::timed(this, "Check locator", "Sunrise/Sunset Error", 2500);
        return;
    }
    m_sunAnnounce = announce;
    m_sunForLocator = locator4();
    ui->sunlabel->setText("Updating sunrise/sunset times... please wait");
    ui->sunlabel->adjustSize();
    const QByteArray tz = QTimeZone::systemTimeZoneId();
    m_sunFetch->fetchYear(ll->lat, ll->lng, QDate::currentDate().year(), QString::fromUtf8(tz));
}

void MainWindow::sunTimesFetched(bool ok, const QMap<QString, SunTimes> &days, const QString &error)
{
    QString text;
    if (!ok) {
        text = "Sunrise/sunset times not updated: " + error;
    } else if (!m_store.saveSunTimes(m_sunForLocator, days)) {
        text = "Error while saving sunrise/sunset times: " + m_store.lastError();
    } else {
        m_sunLocator.clear();   // re-read the cache
        text = (days.size() >= 365 ? QString("Sunrise/sunset times correct for this location")
                                   : QString("Sunrise/sunset times updated for %1 days").arg(days.size()))
               + ".  " + Sun::kAttribution;
        if (m_slotModel && m_slotModel->date().isValid())
            updateSunLabels(m_slotModel->date(), m_slotModel->hour());
    }
    ui->sunlabel->setText(text);
    ui->sunlabel->adjustSize();
    Rx::debugLog("Sun: " + text);
    if (m_sunAnnounce || !ok)
        Msg::timed(this, ok ? "Sunrise/set times updated successfully" : text, "Sunrise/set", 3000);
}

void MainWindow::sunrisebutton_Click()
{
    updateSunTimes(true);
}

// C#: SyncSystemClockWithNTP - runs on a worker thread (the check, and w32tm, can take seconds)
void MainWindow::syncClock(bool announce)
{
    if (m_clockSyncRunning)
        return;
    m_clockSyncRunning = true;
    ui->syncbutton->setEnabled(false);
    auto result = std::make_shared<Ntp::SyncResult>();
    QThread *t = QThread::create([result] { *result = Ntp::syncIfNeeded(); });
    connect(t, &QThread::finished, this, [this, t, result, announce] {
        t->deleteLater();
        m_clockSyncRunning = false;
        ui->syncbutton->setEnabled(true);
        statusBar()->showMessage(QDateTime::currentDateTimeUtc().toString("HH:mm:ss ") + result->message, 60000);
        Rx::debugLog("NTP: " + result->message);
        if (announce || result->attempted || !result->ok)
            Msg::timed(this, result->message, "Clock sync", announce ? 4000 : 6000);
    });
    t->start();
}

void MainWindow::syncbutton_Click()
{
    syncClock(true);
}

// Once a second from daytimer_Tick
void MainWindow::clockAndSunTick(const QDateTime &utc)
{
    if (m_stopInternet)
        return;   // portable: no automatic internet use
    const QTime t = utc.time();
    // daily clock check at 03:15:53 UTC: after a transmission that started at 03:14 has ended (:52),
    // before the next slot is looked up (:54)
    if (t.hour() == 3 && t.minute() == 15 && t.second() == 53)
        syncClock(false);
    // C#: every 30 days at 02:11:20, refresh the sunrise/sunset table
    if (utc.date().dayOfYear() % 30 == 0 && t.hour() == 2 && t.minute() == 11 && t.second() == 20)
        updateSunTimes(false);
}

void MainWindow::AlphacomboBox_SelectedValueChanged(int index)
{
    bool ok = false;
    const double a = ui->AlphacomboBox->itemText(index).toDouble(&ok);
    if (!ok || a <= 0.0 || a > 1.0 || qFuzzyCompare(a, m_cfg.alpha))
        return;
    m_cfg.alpha = a;
    if (m_store.saveAlpha(m_cfg.configId, a))
        statusBar()->showMessage(QString("Gaussian filter alpha %1 saved (used from the next transmission)").arg(a), 10000);
    else
        Msg::timed(this, "Alpha not saved: " + m_store.lastError(), "TX Configuration", 3000);
}

void MainWindow::comboBox1_TextChanged(const QString &) {}   // the list can't be typed in
