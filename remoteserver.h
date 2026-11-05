#pragma once
// Remote tab: a small HTTPS server so TX (and the schedule) can be switched on or off from a browser
// on a phone, tablet or PC - and, if allowed, a frequency and antenna picked and the next slot used.
// C#: RemoteTabWiring / RemoteCallbacks.
//
// Login needs the username, password and passcode. Only salted PBKDF2 hashes of the password and
// passcode are kept. A self-signed certificate is made the first time (the browser warns about it once).
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSslConfiguration>
#include <QString>
#include <QStringList>
#include <functional>

class QSslServer;
class QTcpSocket;

struct RemoteConfig
{
    bool enabled = false;          // "Enable remote access"
    bool allowTxRx = false;        // "Allow remote TX/RX control" (frequency, antenna, Go next slot)
    int port = 5029;
    QString user;
    QByteArray passwordSalt, passwordHash;   // hex
    QByteArray passcodeSalt, passcodeHash;   // hex
    QString pfxPassword;           // protects the generated certificate file
    bool hasSecrets() const { return !passwordHash.isEmpty() && !passcodeHash.isEmpty(); }
};

namespace Remote {
QString configDir();                                       // ~/WSPRSkedRemote
bool loadConfig(const QString &dir, RemoteConfig *cfg);    // false if there is no file yet
bool saveConfig(const QString &dir, const RemoteConfig &cfg, QString *error);
void setSecret(QByteArray *salt, QByteArray *hash, const QString &secret);
bool checkSecret(const QByteArray &salt, const QByteArray &hash, const QString &secret);
// remote.pfx in dir, made with openssl (or PowerShell on Windows) if it isn't there
bool loadOrCreateCertificate(const QString &dir, RemoteConfig *cfg, QSslConfiguration *ssl, QString *error);
QStringList localUrls(int port);                           // https://<each LAN address>:<port>
QString htmlEscape(const QString &s);
}

struct RemoteCallbacks
{
    std::function<QString()> title;            // "WSPR Scheduler - G4GCI"
    std::function<QString()> status;           // several lines
    std::function<QString()> enableTx, disableTx, enableSked, disableSked;
    std::function<bool()> txEnabled, skedEnabled;
    std::function<QStringList()> frequencies;  // "14.0956"
    std::function<QStringList()> antennas;     // "1: Windom"
    std::function<QString(const QString &freqMHz, const QString &antenna)> txNext;   // "Go next slot"
};

class RemoteServer : public QObject
{
    Q_OBJECT
public:
    static constexpr int kMaxFailures = 5;           // per address, then locked out
    static constexpr int kLockoutSecs = 15 * 60;
    static constexpr int kSessionIdleSecs = 15 * 60;
    static constexpr int kMaxSessions = 8;
    static constexpr int kMaxConnections = 16;
    static constexpr int kMaxRequestBytes = 16 * 1024;

    explicit RemoteServer(QObject *parent = nullptr);
    ~RemoteServer() override;

    void setCallbacks(const RemoteCallbacks &cb) { m_cb = cb; }
    bool start(const RemoteConfig &cfg, const QSslConfiguration &ssl, QString *error);
    void stop();
    bool isListening() const;
    int port() const;
    int sessionCount() const { return int(m_sessions.size()); }
    void setFailureDelayMs(int ms) { m_failDelayMs = ms; }   // tests

signals:
    void activity(const QString &line);   // "12:01:05 192.168.1.20 TX disabled"

private:
    struct Session
    {
        QString csrf;
        QDateTime lastUsed;
        QString message;     // shown once on the next page
        QString freq, ant;   // last choice
        QString address;
    };
    struct Failures
    {
        int count = 0;
        QDateTime lockedUntil;
    };
    struct Request
    {
        QString method, path;
        QHash<QString, QString> headers;   // lower-case names
        QHash<QString, QString> form;
    };

    void acceptConnections();
    void readRequest(QTcpSocket *s);
    void handle(QTcpSocket *s, const Request &r);
    void doAction(Session &ses, const QString &address, const Request &r);
    QString sessionToken(const Request &r) const;
    void send(QTcpSocket *s, int code, const QByteArray &body, const QList<QByteArray> &extraHeaders = {});
    void redirectHome(QTcpSocket *s, const QList<QByteArray> &extraHeaders = {});
    QByteArray loginPage(const QString &message) const;
    QByteArray controlPage(const Session &ses) const;
    void expireSessions();
    void log(const QString &address, const QString &what);

    QSslServer *m_server = nullptr;
    RemoteConfig m_cfg;
    RemoteCallbacks m_cb;
    QHash<QString, Session> m_sessions;       // token -> session
    QHash<QString, Failures> m_failures;      // address -> failed logins
    QHash<QTcpSocket *, QByteArray> m_buffers;
    int m_failDelayMs = 1000;
};
