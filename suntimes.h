#pragma once
// Sunrise/sunset times for the station's locator - port of Form1.SaveSunRiseSunset / Sunrise_Sunset /
// Save_SunSet_SunRise (wspr_grey.sunrise_sunset, local times "HH:mm" keyed by locator and "MM-dd").
//
// Differences from the C# version:
//  - one request for the whole year (sunrise-sunset.org API v2, date_start/date_end), not 365 requests
//    to API v1 (v2's terms ask for a range request; v1 rate-limits);
//  - times are asked for in the PC's own time zone (tz=Europe/London etc.), so they are local wall-clock
//    times for each date, summer time included, as C# stored them;
//  - attribution: the API's terms require "Sun data by Sunrise-Sunset.org" to be shown.
#include "slotstore.h"

#include <QByteArray>
#include <QDate>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace Sun {

inline const char *kAttribution = "Sun data by Sunrise-Sunset.org";

// https://api.sunrise-sunset.org/v2?lat=..&lng=..&date_start=YYYY-01-01&date_end=YYYY-12-31&tz=..
QUrl yearUrl(double lat, double lng, int year, const QString &ianaTz,
             const QString &base = QStringLiteral("https://api.sunrise-sunset.org/v2"));

// Parses a v2 range answer into "MM-dd" -> local sunrise/sunset. Days without a sunrise or sunset
// (polar day/night) are left out. Returns an empty map and sets error on an error body or bad JSON.
QMap<QString, SunTimes> parseYear(const QByteArray &json, QString *error);

} // namespace Sun

// Fetches a year and reports it (asynchronous; one at a time)
class SunFetcher : public QObject
{
    Q_OBJECT
public:
    explicit SunFetcher(QObject *parent = nullptr);
    void setBaseUrl(const QString &url) { m_base = url; }   // tests
    bool busy() const { return m_reply != nullptr; }
    void fetchYear(double lat, double lng, int year, const QString &ianaTz);

signals:
    void finished(bool ok, const QMap<QString, SunTimes> &days, const QString &error);

private:
    QNetworkAccessManager *m_net;
    QNetworkReply *m_reply = nullptr;
    QString m_base = QStringLiteral("https://api.sunrise-sunset.org/v2");
};
