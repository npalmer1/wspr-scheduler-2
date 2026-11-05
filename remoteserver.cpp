// Remote tab HTTPS server - see remoteserver.h.
#include "remoteserver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QPasswordDigestor>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr int kPbkdf2Rounds = 20000;

QByteArray randomHex(int bytes)
{
    QByteArray b(bytes, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(b.data()), bytes / 4);
    return b.toHex();
}

bool sameBytes(const QByteArray &a, const QByteArray &b)   // constant time for equal lengths
{
    if (a.size() != b.size())
        return false;
    unsigned char d = 0;
    for (int i = 0; i < a.size(); ++i)
        d |= static_cast<unsigned char>(a[i] ^ b[i]);
    return d == 0;
}

QString runTool(const QString &program, const QStringList &args, int timeoutMs)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForStarted(5000))
        return program + " would not start";
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        return program + " took too long";
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return program + ": " + QString::fromLocal8Bit(p.readAll()).trimmed().left(300);
    return QString();
}

QString findOpenssl()
{
    QString exe = QStandardPaths::findExecutable("openssl");
#ifdef Q_OS_WIN
    if (exe.isEmpty())
        for (const QString &p : {QStringLiteral("C:/Program Files/Git/mingw64/bin/openssl.exe"),
                                 QStringLiteral("C:/Program Files/Git/usr/bin/openssl.exe"),
                                 QStringLiteral("C:/Program Files/OpenSSL-Win64/bin/openssl.exe")})
            if (QFile::exists(p))
                return p;
#endif
    return exe;
}

QString makeWithOpenssl(const QString &dir, const QString &pfx, const QString &pass)
{
    const QString exe = findOpenssl();
    if (exe.isEmpty())
        return "openssl not found";
    const QString key = QDir(dir).filePath("remote_key.tmp"), crt = QDir(dir).filePath("remote_crt.tmp");
    QString err = runTool(exe, {"req", "-x509", "-newkey", "rsa:2048", "-sha256", "-days", "3650", "-nodes",
                                "-keyout", key, "-out", crt, "-subj", "/CN=WSPR Scheduler"}, 60000);
    if (err.isEmpty())
        err = runTool(exe, {"pkcs12", "-export", "-inkey", key, "-in", crt, "-out", pfx,
                            "-name", "WSPR Scheduler", "-passout", "pass:" + pass}, 30000);
    QFile::remove(key);
    QFile::remove(crt);
    return err;
}

#ifdef Q_OS_WIN
// Windows 10/11 without openssl: PowerShell makes the certificate in the user's store, exports it, removes it
QString makeWithPowerShell(const QString &pfx, const QString &pass)
{
    const QString script = QString(
        "$ErrorActionPreference='Stop';"
        "$c=New-SelfSignedCertificate -Subject 'CN=WSPR Scheduler' -CertStoreLocation Cert:\\CurrentUser\\My "
        "-KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 -KeyExportPolicy Exportable "
        "-NotAfter (Get-Date).AddYears(10);"
        "$p=ConvertTo-SecureString -String '%1' -Force -AsPlainText;"
        "Export-PfxCertificate -Cert $c -FilePath '%2' -Password $p | Out-Null;"
        "Remove-Item -Path ('Cert:\\CurrentUser\\My\\'+$c.Thumbprint) -DeleteKey")
        .arg(pass, QDir::toNativeSeparators(pfx).replace("'", "''"));
    return runTool("powershell.exe", {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script},
                   90000);
}
#endif

QString statusText(int code)
{
    switch (code) {
    case 200: return "OK";
    case 303: return "See Other";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 429: return "Too Many Requests";
    default: return "Error";
    }
}

const char *kStyle =
    "body{font-family:system-ui,Segoe UI,Arial,sans-serif;background:#f5f5dc;margin:0;padding:16px;color:#111}"
    "main{max-width:480px;margin:0 auto}"
    "h1{font-size:1.25em;margin:0 0 12px}"
    "pre{background:#fff;border:1px solid #ccc;padding:10px;white-space:pre-wrap;font-size:.95em;margin:0 0 12px}"
    ".msg{background:#fffbe6;border:1px solid #d4b106;padding:8px 10px;margin:0 0 12px}"
    ".err{background:#fff1f0;border-color:#cf1322}"
    "form{margin:0}"
    "label{display:block;margin:10px 0 4px;font-weight:600}"
    "input,select{width:100%;box-sizing:border-box;font-size:1em;padding:8px}"
    "button{font-size:1em;padding:10px 12px;margin:4px 0;width:100%;border:1px solid #888;border-radius:4px;background:#e8e8e8}"
    ".row{display:flex;gap:8px}.row form{flex:1}"
    ".on{background:#c00;color:#fff}.off{background:#2e7d32;color:#fff}"
    ".go{background:#4169e1;color:#fff}"
    "fieldset{border:1px solid #bbb;margin:12px 0;padding:8px 10px}"
    "small{color:#555}";

QByteArray page(const QString &title, const QString &body)
{
    return QString("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                   "<title>%1</title><style>%2</style></head><body><main>%3</main></body></html>")
        .arg(Remote::htmlEscape(title), QString::fromLatin1(kStyle), body)
        .toUtf8();
}
} // namespace

// ---- settings ------------------------------------------------------------------------------------

namespace Remote {

QString configDir()
{
    const QString test = qEnvironmentVariable("WSPRSKED_REMOTE_DIR");   // tests only
    return test.isEmpty() ? QDir::home().filePath("WSPRSkedRemote") : test;
}

QString htmlEscape(const QString &s)
{
    return s.toHtmlEscaped().replace('\'', "&#39;");
}

bool loadConfig(const QString &dir, RemoteConfig *cfg)
{
    const QString file = QDir(dir).filePath("remote.ini");
    if (!QFile::exists(file))
        return false;
    QSettings s(file, QSettings::IniFormat);
    cfg->enabled = s.value("enabled", false).toBool();
    cfg->allowTxRx = s.value("allowTxRx", false).toBool();
    cfg->port = s.value("port", 5029).toInt();
    if (cfg->port < 1 || cfg->port > 65535)
        cfg->port = 5029;
    cfg->user = s.value("user").toString();
    cfg->passwordSalt = s.value("passwordSalt").toByteArray();
    cfg->passwordHash = s.value("passwordHash").toByteArray();
    cfg->passcodeSalt = s.value("passcodeSalt").toByteArray();
    cfg->passcodeHash = s.value("passcodeHash").toByteArray();
    cfg->pfxPassword = s.value("pfxPassword").toString();
    return true;
}

bool saveConfig(const QString &dir, const RemoteConfig &cfg, QString *error)
{
    if (!QDir().mkpath(dir)) {
        *error = "can't make the folder " + QDir::toNativeSeparators(dir);
        return false;
    }
    QSettings s(QDir(dir).filePath("remote.ini"), QSettings::IniFormat);
    s.setValue("enabled", cfg.enabled);
    s.setValue("allowTxRx", cfg.allowTxRx);
    s.setValue("port", cfg.port);
    s.setValue("user", cfg.user);
    s.setValue("passwordSalt", cfg.passwordSalt);
    s.setValue("passwordHash", cfg.passwordHash);
    s.setValue("passcodeSalt", cfg.passcodeSalt);
    s.setValue("passcodeHash", cfg.passcodeHash);
    s.setValue("pfxPassword", cfg.pfxPassword);
    s.sync();
    if (s.status() != QSettings::NoError) {
        *error = "can't write " + QDir::toNativeSeparators(s.fileName());
        return false;
    }
    return true;
}

void setSecret(QByteArray *salt, QByteArray *hash, const QString &secret)
{
    *salt = randomHex(16);
    *hash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, secret.toUtf8(),
                                               QByteArray::fromHex(*salt), kPbkdf2Rounds, 32).toHex();
}

bool checkSecret(const QByteArray &salt, const QByteArray &hash, const QString &secret)
{
    const QByteArray h = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, secret.toUtf8(),
                                                            QByteArray::fromHex(salt), kPbkdf2Rounds, 32).toHex();
    return !hash.isEmpty() && sameBytes(h, hash);
}

bool loadOrCreateCertificate(const QString &dir, RemoteConfig *cfg, QSslConfiguration *ssl, QString *error)
{
    if (!QSslSocket::supportsSsl()) {
        *error = "this PC has no TLS support for Qt (the TLS plugin folder is missing beside the program)";
        return false;
    }
    const QString pfx = QDir(dir).filePath("remote.pfx");
    if (!QFile::exists(pfx) || cfg->pfxPassword.isEmpty()) {
        if (!QDir().mkpath(dir)) {
            *error = "can't make the folder " + QDir::toNativeSeparators(dir);
            return false;
        }
        QFile::remove(pfx);
        cfg->pfxPassword = QString::fromLatin1(randomHex(12));
        QString err;
#ifdef Q_OS_WIN
        err = makeWithPowerShell(pfx, cfg->pfxPassword);
        if (!err.isEmpty() || !QFile::exists(pfx)) {
            const QString err2 = makeWithOpenssl(dir, pfx, cfg->pfxPassword);
            if (err2.isEmpty())
                err.clear();
            else
                err += "; " + err2;
        }
#else
        err = makeWithOpenssl(dir, pfx, cfg->pfxPassword);
#endif
        if (!err.isEmpty() || !QFile::exists(pfx)) {
            *error = "can't make a certificate: " + (err.isEmpty() ? QString("no file made") : err);
            QFile::remove(pfx);
            cfg->pfxPassword.clear();
            return false;
        }
        QString e;
        saveConfig(dir, *cfg, &e);   // keep the certificate password with the settings
    }
    QFile f(pfx);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = "can't read " + QDir::toNativeSeparators(pfx);
        return false;
    }
    QSslKey key;
    QSslCertificate cert;
    QList<QSslCertificate> chain;
    if (!QSslCertificate::importPkcs12(&f, &key, &cert, &chain, cfg->pfxPassword.toUtf8()) || cert.isNull()) {
        *error = "can't read the certificate " + QDir::toNativeSeparators(pfx)
                 + " - delete it and press Save to make a new one";
        return false;
    }
    QSslConfiguration c = QSslConfiguration::defaultConfiguration();
    c.setLocalCertificate(cert);
    c.setPrivateKey(key);
    c.setPeerVerifyMode(QSslSocket::VerifyNone);
    c.setProtocol(QSsl::TlsV1_2OrLater);
    *ssl = c;
    return true;
}

QStringList localUrls(int port)
{
    QStringList urls;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && !a.isLinkLocal())
            urls << QString("https://%1:%2").arg(a.toString()).arg(port);
    if (urls.isEmpty())
        urls << QString("https://localhost:%1").arg(port);
    return urls;
}

} // namespace Remote

// ---- server --------------------------------------------------------------------------------------

RemoteServer::RemoteServer(QObject *parent) : QObject(parent) {}

RemoteServer::~RemoteServer()
{
    stop();
}

bool RemoteServer::start(const RemoteConfig &cfg, const QSslConfiguration &ssl, QString *error)
{
    stop();
    m_cfg = cfg;
    m_server = new QSslServer(this);
    m_server->setSslConfiguration(ssl);
    m_server->setHandshakeTimeout(8000);
    m_server->setMaxPendingConnections(kMaxConnections);
    connect(m_server, &QSslServer::pendingConnectionAvailable, this, &RemoteServer::acceptConnections);
    if (!m_server->listen(QHostAddress::Any, quint16(cfg.port))) {
        *error = QString("can't listen on port %1: %2").arg(cfg.port).arg(m_server->errorString());
        delete m_server;
        m_server = nullptr;
        return false;
    }
    return true;
}

void RemoteServer::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
        it.key()->deleteLater();
    m_buffers.clear();
    m_sessions.clear();
}

bool RemoteServer::isListening() const
{
    return m_server && m_server->isListening();
}

int RemoteServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void RemoteServer::log(const QString &address, const QString &what)
{
    emit activity(QDateTime::currentDateTimeUtc().toString("HH:mm:ss") + " " + address + " " + what);
}

void RemoteServer::acceptConnections()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *s = m_server->nextPendingConnection();
        if (!s)
            break;
        if (m_buffers.size() >= kMaxConnections) {
            s->abort();
            s->deleteLater();
            continue;
        }
        s->setParent(this);   // deleted with the server
        m_buffers.insert(s, QByteArray());
        connect(s, &QTcpSocket::readyRead, this, [this, s] { readRequest(s); });
        connect(s, &QTcpSocket::disconnected, this, [this, s] {
            m_buffers.remove(s);
            s->deleteLater();
        });
        QTimer::singleShot(15000, s, [s] { s->abort(); });   // slow or idle clients
        readRequest(s);
    }
}

void RemoteServer::readRequest(QTcpSocket *s)
{
    auto it = m_buffers.find(s);
    if (it == m_buffers.end() || s->property("answered").toBool())
        return;   // one request per connection
    it->append(s->readAll());
    if (it->size() > kMaxRequestBytes) {
        send(s, 413, "Request too large");
        return;
    }
    const int end = it->indexOf("\r\n\r\n");
    if (end < 0)
        return;
    Request r;
    const QList<QByteArray> lines = it->left(end).split('\n');
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    if (first.size() != 3 || !first[2].startsWith("HTTP/1.")) {
        send(s, 400, "Bad request");
        return;
    }
    r.method = QString::fromLatin1(first[0]);
    r.path = QUrl(QString::fromLatin1(first[1])).path();
    for (int i = 1; i < lines.size(); ++i) {
        const int c = lines[i].indexOf(':');
        if (c > 0)
            r.headers.insert(QString::fromLatin1(lines[i].left(c)).trimmed().toLower(),
                             QString::fromLatin1(lines[i].mid(c + 1)).trimmed());
    }
    bool ok = true;
    const int len = r.headers.value("content-length", "0").toInt(&ok);
    if (!ok || len < 0) {
        send(s, 400, "Bad request");
        return;
    }
    if (it->size() < end + 4 + len)
        return;   // wait for the rest of the form
    if (len > 0) {
        QByteArray body = it->mid(end + 4, len);
        body.replace('+', ' ');
        const QUrlQuery q(QString::fromUtf8(body));
        for (const auto &kv : q.queryItems(QUrl::FullyDecoded))
            r.form.insert(kv.first, kv.second);
    }
    it->clear();
    s->setProperty("answered", true);
    handle(s, r);
}

void RemoteServer::send(QTcpSocket *s, int code, const QByteArray &body, const QList<QByteArray> &extraHeaders)
{
    QByteArray h = "HTTP/1.1 " + QByteArray::number(code) + ' ' + statusText(code).toLatin1() + "\r\n";
    const bool html = body.startsWith("<!doctype");
    h += html ? "Content-Type: text/html; charset=utf-8\r\n" : "Content-Type: text/plain; charset=utf-8\r\n";
    h += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    h += "Cache-Control: no-store\r\n"
         "X-Frame-Options: DENY\r\n"
         "X-Content-Type-Options: nosniff\r\n"
         "Referrer-Policy: no-referrer\r\n"
         "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'\r\n"
         "Connection: close\r\n";
    for (const QByteArray &x : extraHeaders)
        h += x + "\r\n";
    h += "\r\n";
    s->write(h + body);
    s->setProperty("answered", true);
    s->disconnectFromHost();   // after the write has gone
}

void RemoteServer::redirectHome(QTcpSocket *s, const QList<QByteArray> &extraHeaders)
{
    QList<QByteArray> h = extraHeaders;
    h << "Location: /";
    send(s, 303, QByteArray(), h);
}

QString RemoteServer::sessionToken(const Request &r) const
{
    for (const QString &part : r.headers.value("cookie").split(';')) {
        const QString p = part.trimmed();
        if (p.startsWith("wsprsked="))
            return p.mid(9);
    }
    return QString();
}

void RemoteServer::expireSessions()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = m_sessions.begin(); it != m_sessions.end();)
        it = it->lastUsed.secsTo(now) > kSessionIdleSecs ? m_sessions.erase(it) : std::next(it);
    for (auto it = m_failures.begin(); it != m_failures.end();)
        it = (it->lockedUntil.isValid() ? it->lockedUntil < now : it->count == 0) ? m_failures.erase(it) : std::next(it);
}

void RemoteServer::handle(QTcpSocket *s, const Request &r)
{
    expireSessions();
    bool v4 = false;
    const quint32 ip4 = s->peerAddress().toIPv4Address(&v4);
    const QString address = v4 ? QHostAddress(ip4).toString() : s->peerAddress().toString();
    const QString token = sessionToken(r);
    auto ses = m_sessions.find(token);
    const bool loggedIn = !token.isEmpty() && ses != m_sessions.end();

    if (r.path == "/" && r.method == "GET") {
        if (!loggedIn) {
            send(s, 200, loginPage(QString()));
            return;
        }
        ses->lastUsed = QDateTime::currentDateTimeUtc();
        send(s, 200, controlPage(*ses));
        ses->message.clear();
        return;
    }

    if (r.path == "/login" && r.method == "POST") {
        Failures &f = m_failures[address];
        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (f.lockedUntil.isValid() && f.lockedUntil > now) {
            send(s, 429, loginPage(QString("Too many failed log-ins from %1 - try again after %2 UTC")
                                       .arg(address, f.lockedUntil.toString("HH:mm"))));
            return;
        }
        // all three checked every time, so the time taken says nothing about which was wrong
        const bool userOk = !m_cfg.user.isEmpty()
                            && sameBytes(r.form.value("user").trimmed().toUtf8(), m_cfg.user.toUtf8());
        const bool passOk = Remote::checkSecret(m_cfg.passwordSalt, m_cfg.passwordHash, r.form.value("password"));
        const bool codeOk = Remote::checkSecret(m_cfg.passcodeSalt, m_cfg.passcodeHash, r.form.value("passcode"));
        if (!(userOk && passOk && codeOk)) {
            QString msg = "Username, password or passcode not correct";
            if (++f.count >= kMaxFailures) {
                f.lockedUntil = now.addSecs(kLockoutSecs);
                msg = QString("Too many failed log-ins - locked out until %1 UTC").arg(f.lockedUntil.toString("HH:mm"));
                log(address, QString("locked out after %1 failed log-ins").arg(f.count));
            } else {
                log(address, "log-in failed");
            }
            const QByteArray body = loginPage(msg);
            QPointer<QTcpSocket> p(s);
            QTimer::singleShot(m_failDelayMs, this, [this, p, body] {
                if (p)
                    send(p, 403, body);
            });
            return;
        }
        m_failures.remove(address);
        if (m_sessions.size() >= kMaxSessions) {   // drop the oldest
            auto oldest = m_sessions.begin();
            for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
                if (it->lastUsed < oldest->lastUsed)
                    oldest = it;
            m_sessions.erase(oldest);
        }
        const QString t = QString::fromLatin1(randomHex(32));
        Session n;
        n.csrf = QString::fromLatin1(randomHex(16));
        n.lastUsed = now;
        n.address = address;
        n.message = "Logged in";
        m_sessions.insert(t, n);
        log(address, "logged in");
        redirectHome(s, {"Set-Cookie: wsprsked=" + t.toLatin1() + "; Path=/; Secure; HttpOnly; SameSite=Strict"});
        return;
    }

    if (r.path == "/do" && r.method == "POST") {
        if (!loggedIn) {
            send(s, 403, loginPage("Logged out - please log in again"));
            return;
        }
        if (!sameBytes(r.form.value("csrf").toUtf8(), ses->csrf.toUtf8())) {
            send(s, 403, "Form out of date - reload the page");
            return;
        }
        ses->lastUsed = QDateTime::currentDateTimeUtc();
        if (r.form.value("a") == "logout") {
            m_sessions.erase(ses);
            log(address, "logged out");
            redirectHome(s, {"Set-Cookie: wsprsked=; Path=/; Max-Age=0; Secure; HttpOnly; SameSite=Strict"});
            return;
        }
        doAction(*ses, address, r);
        redirectHome(s);
        return;
    }

    if (r.path == "/" || r.path == "/login" || r.path == "/do")
        send(s, 405, "Method not allowed");
    else
        send(s, 404, "Not found");
}

void RemoteServer::doAction(Session &ses, const QString &address, const Request &r)
{
    const QString a = r.form.value("a");
    QString result;
    if (a == "status") {
        result = "Status updated";
    } else if (a == "txon" && m_cb.enableTx) {
        result = m_cb.enableTx();
    } else if (a == "txoff" && m_cb.disableTx) {
        result = m_cb.disableTx();
    } else if (a == "skedoff" && m_cb.disableSked) {
        result = m_cb.disableSked();
    } else if (a == "skedon" && m_cb.enableSked) {
        result = m_cb.enableSked();
    } else if (a == "go") {
        ses.freq = r.form.value("freq").trimmed();
        ses.ant = r.form.value("ant").trimmed();
        if (!m_cfg.allowTxRx)
            result = "Remote TX/RX control is not allowed (tick \"Allow remote TX/RX control\" on the Remote tab)";
        else if (!m_cb.frequencies || !m_cb.frequencies().contains(ses.freq))
            result = "Choose a frequency from the list";
        else if (!m_cb.antennas || !m_cb.antennas().contains(ses.ant))
            result = "Choose an antenna from the list";
        else if (m_cb.txNext)
            result = m_cb.txNext(ses.freq, ses.ant);
    } else {
        result = "Unknown request";
    }
    ses.message = result;
    if (a != "status")
        log(address, result);
}

QByteArray RemoteServer::loginPage(const QString &message) const
{
    const QString title = m_cb.title ? m_cb.title() : QString("WSPR Scheduler");
    QString b = "<h1>" + Remote::htmlEscape(title) + "</h1>";
    if (!message.isEmpty())
        b += "<p class=\"msg err\">" + Remote::htmlEscape(message) + "</p>";
    b += "<form method=\"post\" action=\"/login\">"
         "<label for=\"u\">Username</label><input id=\"u\" name=\"user\" autocomplete=\"username\" autocapitalize=\"none\" required>"
         "<label for=\"p\">Password</label><input id=\"p\" name=\"password\" type=\"password\" autocomplete=\"current-password\" required>"
         "<label for=\"c\">Passcode</label><input id=\"c\" name=\"passcode\" type=\"password\" inputmode=\"numeric\" autocomplete=\"off\" required>"
         "<p><button class=\"go\" type=\"submit\">Log in</button></p></form>";
    return page(title, b);
}

QByteArray RemoteServer::controlPage(const Session &ses) const
{
    const QString title = m_cb.title ? m_cb.title() : QString("WSPR Scheduler");
    const QString csrf = "<input type=\"hidden\" name=\"csrf\" value=\"" + ses.csrf + "\">";
    auto button = [&](const QString &action, const QString &text, const QString &cls) {
        return "<form method=\"post\" action=\"/do\">" + csrf + "<input type=\"hidden\" name=\"a\" value=\"" + action
               + "\"><button class=\"" + cls + "\" type=\"submit\">" + Remote::htmlEscape(text) + "</button></form>";
    };
    QString b = "<h1>" + Remote::htmlEscape(title) + "</h1>";
    if (!ses.message.isEmpty())
        b += "<p class=\"msg\">" + Remote::htmlEscape(ses.message) + "</p>";
    b += "<pre>" + Remote::htmlEscape(m_cb.status ? m_cb.status() : QString()) + "</pre>";
    b += button("status", "Status", "");
    const bool tx = m_cb.txEnabled && m_cb.txEnabled();   // red while TX is on, green while off (as the desktop button)
    const bool sked = m_cb.skedEnabled && m_cb.skedEnabled();
    b += "<div class=\"row\">" + (tx ? button("txoff", "Disable TX", "on") : button("txon", "Enable TX", "off"))
         + (sked ? button("skedoff", "Disable sked", "") : button("skedon", "Enable sked", "")) + "</div>";

    if (m_cfg.allowTxRx) {
        b += "<fieldset><form method=\"post\" action=\"/do\">" + csrf + "<input type=\"hidden\" name=\"a\" value=\"go\">";
        b += "<label for=\"f\">Frequency (MHz)</label><select id=\"f\" name=\"freq\">";
        for (const QString &f : m_cb.frequencies ? m_cb.frequencies() : QStringList())
            b += "<option" + QString(f == ses.freq ? " selected" : "") + ">" + Remote::htmlEscape(f) + "</option>";
        b += "</select><label for=\"an\">Antenna</label><select id=\"an\" name=\"ant\">";
        for (const QString &a : m_cb.antennas ? m_cb.antennas() : QStringList())
            b += "<option" + QString(a == ses.ant ? " selected" : "") + ">" + Remote::htmlEscape(a) + "</option>";
        b += "</select><p><button class=\"go\" type=\"submit\">Go next slot</button></p>"
             "<small>Disables the schedule. Transmits in the next 2-minute slot if TX is enabled, "
             "otherwise receives on that frequency.</small></form></fieldset>";
    }
    b += button("logout", "Log out", "");
    return page(title, b);
}
