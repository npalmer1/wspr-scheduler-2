#include "suntimes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace Sun {

QUrl yearUrl(double lat, double lng, int year, const QString &ianaTz, const QString &base)
{
    QUrl u(base);
    QUrlQuery q;
    q.addQueryItem("lat", QString::number(lat, 'f', 4));
    q.addQueryItem("lng", QString::number(lng, 'f', 4));
    q.addQueryItem("date_start", QDate(year, 1, 1).toString("yyyy-MM-dd"));
    q.addQueryItem("date_end", QDate(year, 12, 31).toString("yyyy-MM-dd"));
    if (!ianaTz.isEmpty())
        q.addQueryItem("tz", ianaTz);
    u.setQuery(q);
    return u;
}

// "2026-01-15T08:29:18+01:00" -> 08:29 (the wall-clock time in the requested zone; truncated to the
// minute, as C# DateTime.ToString("HH:mm"))
static QTime wallTime(const QJsonValue &v)
{
    const QString s = v.toString();
    if (s.size() < 16 || s.at(10) != 'T')
        return {};
    return QTime::fromString(s.mid(11, 5), "HH:mm");
}

QMap<QString, SunTimes> parseYear(const QByteArray &json, QString *error)
{
    QMap<QString, SunTimes> out;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (!doc.isObject()) {
        *error = "not a JSON answer: " + pe.errorString();
        return out;
    }
    const QJsonObject o = doc.object();
    if (o.contains("error")) {
        *error = o.value("message").toString(o.value("error").toString());
        return out;
    }
    QJsonArray days = o.value("days").toArray();
    if (days.isEmpty() && o.contains("sunrise"))   // a single-date answer
        days.append(o);
    for (const QJsonValue &dv : std::as_const(days)) {
        const QJsonObject d = dv.toObject();
        const QDate date = QDate::fromString(d.value("date").toString(), "yyyy-MM-dd");
        const QTime rise = wallTime(d.value("sunrise")), set = wallTime(d.value("sunset"));
        if (date.isValid() && rise.isValid() && set.isValid())
            out.insert(date.toString("MM-dd"), SunTimes{rise, set});
    }
    if (out.isEmpty())
        *error = "no sunrise/sunset times in the answer";
    return out;
}

} // namespace Sun

SunFetcher::SunFetcher(QObject *parent) : QObject(parent), m_net(new QNetworkAccessManager(this)) {}

void SunFetcher::fetchYear(double lat, double lng, int year, const QString &ianaTz)
{
    if (m_reply)
        return;
    QNetworkRequest req(Sun::yearUrl(lat, lng, year, ianaTz, m_base));
    req.setHeader(QNetworkRequest::UserAgentHeader, "WSPR-Sked");
    req.setTransferTimeout(30000);
    m_reply = m_net->get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *r = m_reply;
        m_reply = nullptr;
        r->deleteLater();
        const QByteArray body = r->readAll();
        QString err;
        QMap<QString, SunTimes> days;
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError && body.isEmpty()) {
            err = r->errorString();
        } else if (status == 429) {
            err = "the sunrise/sunset service is busy (too many requests) - try again later";
        } else {
            days = Sun::parseYear(body, &err);
        }
        emit finished(!days.isEmpty(), days, err);
    });
}
