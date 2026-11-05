// RemoteServer: certificate made with openssl, HTTPS log-in (username + password + passcode), lock-out,
// session cookie, form token, TX/sked buttons, Go next slot only when allowed, log-out.
// The server runs in its own thread; the "browser" here is a blocking QSslSocket.
#include "remoteserver.h"

#include <QCoreApplication>
#include <QDir>
#include <QMutex>
#include <QRegularExpression>
#include <QSslKey>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct State
{
    QMutex lock;
    bool tx = false, sked = true;
    QStringList calls;
};

struct Reply
{
    int code = 0;
    QString headers, body;
    QString cookie() const
    {
        const auto m = QRegularExpression("Set-Cookie: wsprsked=([0-9a-f]*)").match(headers);
        return m.hasMatch() ? m.captured(1) : QString("-");
    }
};

static int g_port = 0;

static Reply request(const QString &method, const QString &path, const QString &cookie = QString(),
                     const QByteArray &form = QByteArray(), const QByteArray &raw = QByteArray())
{
    QSslSocket s;
    s.setPeerVerifyMode(QSslSocket::VerifyNone);   // self-signed, as a browser told to carry on
    s.connectToHostEncrypted("127.0.0.1", quint16(g_port));
    Reply r;
    if (!s.waitForEncrypted(10000)) {
        std::printf("     TLS: %s\n", qPrintable(s.errorString()));
        return r;
    }
    QByteArray req = raw;
    if (req.isEmpty()) {
        req = method.toLatin1() + ' ' + path.toLatin1() + " HTTP/1.1\r\nHost: test\r\n";
        if (!cookie.isEmpty())
            req += "Cookie: other=1; wsprsked=" + cookie.toLatin1() + "\r\n";
        if (!form.isEmpty())
            req += "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: " + QByteArray::number(form.size()) + "\r\n";
        req += "\r\n" + form;
    }
    s.write(req);
    QByteArray all;
    while (s.waitForReadyRead(10000))
        all += s.readAll();
    all += s.readAll();
    const int end = all.indexOf("\r\n\r\n");
    r.headers = QString::fromUtf8(all.left(end));
    r.body = QString::fromUtf8(all.mid(end + 4));
    r.code = r.headers.section(' ', 1, 1).toInt();
    return r;
}

static QString csrfOf(const QString &body)
{
    const auto m = QRegularExpression("name=\"csrf\" value=\"([0-9a-f]+)\"").match(body);
    return m.hasMatch() ? m.captured(1) : QString();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;

    // ---- settings and hashes ----
    RemoteConfig cfg;
    cfg.enabled = true;
    cfg.port = 0;
    cfg.user = "g4gci";
    Remote::setSecret(&cfg.passwordSalt, &cfg.passwordHash, "s3cret pass");
    Remote::setSecret(&cfg.passcodeSalt, &cfg.passcodeHash, "4711");
    CHECK(cfg.hasSecrets() && !cfg.passwordHash.contains("s3cret"));
    CHECK(Remote::checkSecret(cfg.passwordSalt, cfg.passwordHash, "s3cret pass"));
    CHECK(!Remote::checkSecret(cfg.passwordSalt, cfg.passwordHash, "s3cret Pass"));
    CHECK(!Remote::checkSecret(QByteArray(), QByteArray(), ""));
    RemoteConfig c2 = cfg;
    Remote::setSecret(&c2.passwordSalt, &c2.passwordHash, "s3cret pass");
    CHECK(c2.passwordHash != cfg.passwordHash);   // salted
    CHECK(Remote::htmlEscape("<b>\"x\"&'") == "&lt;b&gt;&quot;x&quot;&amp;&#39;");

    // ---- certificate ----
    QSslConfiguration ssl;
    QString err;
    CHECK(Remote::loadOrCreateCertificate(dir.path(), &cfg, &ssl, &err));
    if (!err.isEmpty())
        std::printf("     certificate: %s\n", qPrintable(err));
    CHECK(QFile::exists(QDir(dir.path()).filePath("remote.pfx")) && !cfg.pfxPassword.isEmpty());
    CHECK(!ssl.localCertificate().isNull() && !ssl.privateKey().isNull());
    CHECK(QDir(dir.path()).entryList(QDir::Files).filter(".tmp").isEmpty());   // PEM files removed
    const QString certSerial = QString::fromLatin1(ssl.localCertificate().serialNumber());
    RemoteConfig reloaded;
    CHECK(Remote::loadConfig(dir.path(), &reloaded) && reloaded.pfxPassword == cfg.pfxPassword);
    CHECK(reloaded.user == "g4gci" && reloaded.passcodeHash == cfg.passcodeHash);
    QSslConfiguration ssl2;
    CHECK(Remote::loadOrCreateCertificate(dir.path(), &reloaded, &ssl2, &err)
          && QString::fromLatin1(ssl2.localCertificate().serialNumber()) == certSerial);   // kept, not remade
    RemoteConfig wrong = reloaded;
    wrong.pfxPassword = "nope";
    CHECK(!Remote::loadOrCreateCertificate(dir.path(), &wrong, &ssl2, &err) && err.contains("delete it"));
    CHECK(!Remote::localUrls(5029).isEmpty() && Remote::localUrls(5029).first().endsWith(":5029"));

    // ---- server in its own thread ----
    State st;
    QThread thread;
    RemoteServer *server = new RemoteServer;
    server->moveToThread(&thread);
    QStringList activity;
    QMutex actLock;
    QObject::connect(server, &RemoteServer::activity, server, [&](const QString &l) {
        QMutexLocker k(&actLock);
        activity << l;
    }, Qt::DirectConnection);
    RemoteCallbacks cb;
    cb.title = [] { return QString("WSPR Scheduler - G4GCI"); };
    cb.status = [&] { QMutexLocker k(&st.lock); return QString("TX %1\nSked <%2>").arg(st.tx ? "ENABLED" : "disabled", st.sked ? "on" : "off"); };
    cb.enableTx = [&] { QMutexLocker k(&st.lock); st.tx = true; st.calls << "txon"; return QString("TX enabled"); };
    cb.disableTx = [&] { QMutexLocker k(&st.lock); st.tx = false; st.calls << "txoff"; return QString("TX disabled"); };
    cb.enableSked = [&] { QMutexLocker k(&st.lock); st.sked = true; st.calls << "skedon"; return QString("Schedule enabled"); };
    cb.disableSked = [&] { QMutexLocker k(&st.lock); st.sked = false; st.calls << "skedoff"; return QString("Schedule disabled"); };
    cb.txEnabled = [&] { QMutexLocker k(&st.lock); return st.tx; };
    cb.skedEnabled = [&] { QMutexLocker k(&st.lock); return st.sked; };
    cb.frequencies = [] { return QStringList{"7.0386", "14.0956"}; };
    cb.antennas = [] { return QStringList{"1: Windom", "2: Vertical & <loop>"}; };
    cb.txNext = [&](const QString &f, const QString &a) { QMutexLocker k(&st.lock); st.calls << "go " + f + " " + a; return "TX armed for " + f; };
    server->setCallbacks(cb);
    server->setFailureDelayMs(50);
    thread.start();
    bool started = false;
    QMetaObject::invokeMethod(server, [&] { started = server->start(cfg, ssl, &err); g_port = server->port(); },
                              Qt::BlockingQueuedConnection);
    CHECK(started && g_port > 0);
    std::printf("     listening on %d\n", g_port);

    // ---- not logged in ----
    Reply r = request("GET", "/");
    CHECK(r.code == 200 && r.body.contains("name=\"passcode\"") && r.body.contains("WSPR Scheduler - G4GCI"));
    CHECK(r.headers.contains("Cache-Control: no-store") && r.headers.contains("Content-Security-Policy"));
    CHECK(request("GET", "/nothing").code == 404);
    CHECK(request("GET", "/do").code == 405);
    r = request("POST", "/do", QString(), "a=txon");
    CHECK(r.code == 403 && st.calls.isEmpty());
    r = request("POST", "/do", "0123abcd", "a=txon");          // made-up cookie
    CHECK(r.code == 403 && st.calls.isEmpty());
    CHECK(request("GET", "/", QString(), QByteArray(), "GARBAGE\r\n\r\n").code == 400);
    CHECK(request("GET", "/", QString(), QByteArray(), "POST /login HTTP/1.1\r\nContent-Length: 99999\r\n\r\n"
                                                        + QByteArray(17000, 'x')).code == 413);

    // ---- log-in: every part must be right ----
    r = request("POST", "/login", QString(), "user=g4gci&password=s3cret+pass&passcode=0000");
    CHECK(r.code == 403 && r.body.contains("not correct") && r.cookie() == "-");
    r = request("POST", "/login", QString(), "user=G4XYZ&password=s3cret+pass&passcode=4711");
    CHECK(r.code == 403);
    r = request("POST", "/login", QString(), "user=g4gci&password=s3cret%20pass&passcode=4711");
    CHECK(r.code == 303 && r.headers.contains("Location: /") && r.cookie().size() == 64);
    CHECK(r.headers.contains("Secure") && r.headers.contains("HttpOnly") && r.headers.contains("SameSite=Strict"));
    const QString cookie = r.cookie();

    r = request("GET", "/", cookie);
    CHECK(r.code == 200 && r.body.contains("Logged in") && r.body.contains("TX disabled") && r.body.contains("Sked &lt;on&gt;"));
    CHECK(r.body.contains(">Enable TX<") && r.body.contains(">Disable sked<") && r.body.contains("Log out"));
    CHECK(!r.body.contains("Go next slot"));                   // TX/RX control not allowed
    const QString csrf = csrfOf(r.body);
    CHECK(csrf.size() == 32);
    CHECK(!request("GET", "/", cookie).body.contains("Logged in"));   // message shown once

    // ---- buttons ----
    r = request("POST", "/do", cookie, "a=txon");                       // no form token
    CHECK(r.code == 403 && st.calls.isEmpty());
    r = request("POST", "/do", cookie, "a=txon&csrf=" + csrf.toLatin1());
    CHECK(r.code == 303 && st.calls == QStringList{"txon"});
    r = request("GET", "/", cookie);
    CHECK(r.body.contains("TX enabled") && r.body.contains(">Disable TX<"));
    request("POST", "/do", cookie, "a=skedoff&csrf=" + csrf.toLatin1());
    request("POST", "/do", cookie, "a=txoff&csrf=" + csrf.toLatin1());
    request("POST", "/do", cookie, "a=skedon&csrf=" + csrf.toLatin1());
    CHECK(st.calls == (QStringList{"txon", "skedoff", "txoff", "skedon"}));
    request("POST", "/do", cookie, "a=go&freq=14.0956&ant=1%3A+Windom&csrf=" + csrf.toLatin1());
    CHECK(st.calls.size() == 4 && request("GET", "/", cookie).body.contains("not allowed"));
    request("POST", "/do", cookie, "a=status&csrf=" + csrf.toLatin1());
    CHECK(request("GET", "/", cookie).body.contains("Status updated"));

    // ---- Go next slot when allowed ----
    QMetaObject::invokeMethod(server, [&] { server->stop(); cfg.allowTxRx = true; cfg.port = g_port;
                                            started = server->start(cfg, ssl, &err); }, Qt::BlockingQueuedConnection);
    CHECK(started);
    CHECK(request("GET", "/", cookie).code == 200 && request("GET", "/", cookie).body.contains("name=\"passcode\""));   // restart = log in again
    const QString c3 = request("POST", "/login", QString(), "user=g4gci&password=s3cret+pass&passcode=4711").cookie();
    r = request("GET", "/", c3);
    const QString csrf3 = csrfOf(r.body);
    CHECK(r.body.contains("Go next slot") && r.body.contains("<option>14.0956</option>"));
    CHECK(r.body.contains("2: Vertical &amp; &lt;loop&gt;"));   // escaped
    request("POST", "/do", c3, "a=go&freq=21.0946&ant=1%3A+Windom&csrf=" + csrf3.toLatin1());
    CHECK(request("GET", "/", c3).body.contains("Choose a frequency"));
    request("POST", "/do", c3, "a=go&freq=14.0956&ant=9%3A+Nothing&csrf=" + csrf3.toLatin1());
    CHECK(request("GET", "/", c3).body.contains("Choose an antenna"));
    request("POST", "/do", c3, "a=go&freq=14.0956&ant=2%3A+Vertical+%26+%3Cloop%3E&csrf=" + csrf3.toLatin1());
    CHECK(st.calls.last() == "go 14.0956 2: Vertical & <loop>");
    r = request("GET", "/", c3);
    CHECK(r.body.contains("TX armed for 14.0956") && r.body.contains("<option selected>14.0956</option>"));
    if (!qEnvironmentVariableIsEmpty("REMOTE_DUMP_DIR")) {   // for looking at the pages in a browser
        QFile c(QDir(qEnvironmentVariable("REMOTE_DUMP_DIR")).filePath("control.html"));
        if (c.open(QIODevice::WriteOnly)) c.write(r.body.toUtf8());
        QFile l(QDir(qEnvironmentVariable("REMOTE_DUMP_DIR")).filePath("login.html"));
        if (l.open(QIODevice::WriteOnly)) l.write(request("POST", "/login", QString(), "user=x&password=y&passcode=z").body.toUtf8());
    }

    // ---- log out ----
    r = request("POST", "/do", c3, "a=logout&csrf=" + csrf3.toLatin1());
    CHECK(r.code == 303 && r.headers.contains("Max-Age=0"));
    CHECK(request("GET", "/", c3).body.contains("name=\"passcode\""));
    CHECK(request("POST", "/do", c3, "a=txon&csrf=" + csrf3.toLatin1()).code == 403);

    // ---- lock-out after 5 failures from one address, even with the right details ----
    for (int i = 0; i < RemoteServer::kMaxFailures; ++i)
        request("POST", "/login", QString(), "user=g4gci&password=guess&passcode=1");
    r = request("POST", "/login", QString(), "user=g4gci&password=s3cret+pass&passcode=4711");
    CHECK(r.code == 429 && r.body.contains("try again after") && r.cookie() == "-");
    {
        QMutexLocker k(&actLock);
        CHECK(activity.filter("locked out").size() == 1 && activity.filter("logged in").size() == 2);
        CHECK(activity.filter("TX enabled").size() == 1);
        std::printf("     activity: %s\n", qPrintable(activity.join(" | ")));
    }

    QMetaObject::invokeMethod(server, [&] { delete server; }, Qt::BlockingQueuedConnection);
    thread.quit();
    thread.wait();
    std::printf(failures ? "remote_test: %d FAILED\n" : "remote_test: all passed\n", failures);
    return failures ? 1 : 0;
}
