#pragma once
// Spot upload to wsprnet.org (C#: RXForm.Post_wsprdata).
// One shared QNetworkAccessManager, 10 s limit per spot, result written to the debug log as C# did.
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

class WsprnetUpload : public QObject
{
    Q_OBJECT
public:
    explicit WsprnetUpload(QObject *parent = nullptr);

    void setUrl(const QUrl &url) { m_url = url; }      // default http://wsprnet.org/post/
    void setUserAgent(const QString &ua) { m_userAgent = ua; }
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }

    // Posts the form; tag (usually the call) is only used for the log and the signal
    void post(const QList<QPair<QString, QString>> &form, const QString &tag);
    int pending() const { return m_pending; }

signals:
    // httpStatus 0 = no answer (timeout, DNS, refused); body is the first 120 characters
    void finished(const QString &tag, int httpStatus, const QString &body);

private:
    QNetworkAccessManager *m_nam;
    QUrl m_url{"http://wsprnet.org/post/"};
    QString m_userAgent;
    int m_timeoutMs = 10000;
    int m_pending = 0;
};
