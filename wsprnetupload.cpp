#include "wsprnetupload.h"

#include "rxspot.h"

#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

WsprnetUpload::WsprnetUpload(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
    m_nam->setProxy(QNetworkProxy::NoProxy);   // C#: UseProxy = false
}

void WsprnetUpload::post(const QList<QPair<QString, QString>> &form, const QString &tag)
{
    QUrlQuery q;
    for (const auto &kv : form)
        q.addQueryItem(kv.first, kv.second);
    // application/x-www-form-urlencoded: '+' and '/' must be encoded (FormUrlEncodedContent did that)
    QByteArray body = q.query(QUrl::FullyEncoded).toUtf8();
    body.replace("+", "%2B");

    QNetworkRequest req(m_url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    if (!m_userAgent.isEmpty())
        req.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    req.setTransferTimeout(m_timeoutMs);

    ++m_pending;
    QNetworkReply *reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag] {
        reply->deleteLater();
        --m_pending;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString text;
        if (status > 0) {
            text = QString::fromUtf8(reply->readAll().left(120));
            text.replace('\r', ' ').replace('\n', ' ');
            Rx::debugLog(QString("UPLOAD %1 %2 host=%3 body=%4").arg(status).arg(tag, m_url.host(), text));
        } else {
            text = reply->errorString();
            Rx::debugLog(QString("UPLOAD EXCEPTION %1: %2").arg(tag, text));
        }
        emit finished(tag, status, text);
    });
}
