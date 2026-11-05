// Sunrise/sunset fetch (fake sunrise-sunset.org server) and the NTP clock check (fake time server).
// The database part runs only when the scratch MariaDB is there.
#include "ntpcheck.h"
#include "suntimes.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QUrlQuery>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstring>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const char *kYear = R"({"tzid":"Europe/London","lat":50.85,"lng":-0.95,"days":[
 {"date":"2026-01-01","sunrise":"2026-01-01T08:03:41+00:00","sunset":"2026-01-01T16:12:09+00:00"},
 {"date":"2026-06-21","sunrise":"2026-06-21T04:59:30+01:00","sunset":"2026-06-21T21:22:59+01:00"},
 {"date":"2026-11-04","sunrise":"2026-11-04T07:03:00+00:00","sunset":"2026-11-04T16:31:00+00:00"},
 {"date":"2026-12-25","sunrise":null,"sunset":"2026-12-25T16:01:00+00:00"}]})";

// Puts an NTP timestamp for msSince1970 into b[pos]
static void putTs(unsigned char *b, int pos, qint64 ms)
{
    const quint64 secs = quint64(ms / 1000 + 2208988800LL), frac = quint64((ms % 1000) * 4294967296.0 / 1000.0);
    for (int i = 0; i < 4; ++i) { b[pos + i] = uchar(secs >> (24 - 8 * i)); b[pos + 4 + i] = uchar(frac >> (24 - 8 * i)); }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // ---- sun: URL and parsing ----
    const QUrl u = Sun::yearUrl(50.8542, -0.9583, 2026, "Europe/London");
    const QUrlQuery q(u);
    CHECK(u.host() == "api.sunrise-sunset.org" && u.path() == "/v2");
    CHECK(q.queryItemValue("date_start") == "2026-01-01" && q.queryItemValue("date_end") == "2026-12-31");
    CHECK(q.queryItemValue("tz") == "Europe/London" && q.queryItemValue("lat") == "50.8542");
    QString err;
    auto days = Sun::parseYear(kYear, &err);
    CHECK(days.size() == 3);                                             // 25 Dec has no sunrise: left out
    CHECK(days.value("06-21").rise == QTime(4, 59) && days.value("06-21").set == QTime(21, 22));   // BST, truncated
    CHECK(days.value("11-04").rise == QTime(7, 3) && days.value("11-04").set == QTime(16, 31));
    CHECK(Sun::parseYear(R"({"error":"invalid_lat","message":"lat must be -90..90"})", &err).isEmpty()
          && err == "lat must be -90..90");
    CHECK(Sun::parseYear("<html>busy</html>", &err).isEmpty());

    // ---- sun: fetch from a fake server ----
    QTcpServer web;
    web.listen(QHostAddress::LocalHost);
    QString asked;
    QObject::connect(&web, &QTcpServer::newConnection, [&] {
        QTcpSocket *c = web.nextPendingConnection();
        QObject::connect(c, &QTcpSocket::readyRead, c, [&, c] {
            const QByteArray req = c->readAll();
            asked = QString::fromLatin1(req.left(req.indexOf('\r')));
            const QByteArray body = kYear;
            c->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                     + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            c->disconnectFromHost();
        });
    });
    SunFetcher f;
    f.setBaseUrl(QString("http://127.0.0.1:%1/v2").arg(web.serverPort()));
    bool gotOk = false;
    QMap<QString, SunTimes> got;
    QEventLoop loop;
    QObject::connect(&f, &SunFetcher::finished, [&](bool ok, const QMap<QString, SunTimes> &d, const QString &) {
        gotOk = ok; got = d; loop.quit();
    });
    f.fetchYear(50.85, -0.95, 2026, "Europe/London");
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(gotOk && got.size() == 3 && asked.contains("date_start=2026-01-01") && asked.contains("tz=Europe/London"));

    // ---- sun: saved to wspr_grey (scratch database) ----
    SlotStore st;
    if (st.loadConfig()) {
        CHECK(st.saveSunTimes("ZZ99", got));
        const auto back = st.allSunTimes("ZZ99");
        CHECK(back.size() == 3 && back.value("06-21").rise == QTime(4, 59) && back.value("11-04").set == QTime(16, 31));
        got["11-04"].rise = QTime(7, 4);   // an update replaces the day
        CHECK(st.saveSunTimes("ZZ99", got) && st.allSunTimes("ZZ99").value("11-04").rise == QTime(7, 4));
        // alpha
        CHECK(st.saveAlpha(0, 0.05) && qFuzzyCompare(st.loadConfig()->alpha, 0.05));
        CHECK(st.saveAlpha(0, 0.1));
        // database password change, on a throw-away account (needs no grants to change its own password)
        {
            QSqlDatabase adm = QSqlDatabase::addDatabase("QMYSQL", "pwadmin");
            adm.setHostName("127.0.0.1"); adm.setUserName("admin"); adm.setPassword("wspr");
            if (adm.open()) {
                QSqlQuery q(adm);
                q.exec("DROP USER IF EXISTS 'pwtest'@'%'");
                q.exec("CREATE USER 'pwtest'@'%' IDENTIFIED BY 'old1'");
                DbSettings oldS; oldS.user = "pwtest"; oldS.password = "old1";
                DbSettings newS = oldS; newS.password = "New.pass-2";
                CHECK(SlotStore::canLogIn(oldS));
                SlotStore pw(oldS);
                CHECK(!pw.changeOwnPassword("bad'quote"));          // refused before reaching the server
                CHECK(!pw.changeOwnPassword("abc"));                // too short
                CHECK(pw.changeOwnPassword("New.pass-2"));
                std::printf("     password changed (%s)\n", pw.lastError().isEmpty() ? "first try" : qPrintable(pw.lastError()));
                CHECK(SlotStore::canLogIn(newS) && !SlotStore::canLogIn(oldS));
                CHECK(pw.settings().password == "New.pass-2");
                q.exec("DROP USER IF EXISTS 'pwtest'@'%'");
                adm.close();
            } else {
                std::printf("     password change test skipped (no admin login)\n");
            }
        }
        QSqlDatabase::removeDatabase("pwadmin");
    } else {
        std::printf("     no scratch database - store part skipped\n");
    }

    // ---- NTP ----
    unsigned char b[48] = {};
    putTs(b, 40, 1793750400123LL);
    CHECK(qAbs(Ntp::timestampToMs(b, 40) - 1793750400123LL) <= 1);
    CHECK(Ntp::offsetMs(1000, 3500, 3510, 1030) == 2490);   // server 2.49 s ahead, 30 ms round trip

    // fake server, 1.5 s ahead of this PC, in its own thread (query() blocks)
    QThread th;
    QUdpSocket *srv = new QUdpSocket;
    srv->bind(QHostAddress::LocalHost, 0);
    const quint16 port = srv->localPort();
    srv->moveToThread(&th);
    QObject::connect(srv, &QUdpSocket::readyRead, srv, [srv] {
        while (srv->hasPendingDatagrams()) {
            QHostAddress from; quint16 fp;
            unsigned char in[48];
            srv->readDatagram(reinterpret_cast<char *>(in), 48, &from, &fp);
            unsigned char out[48] = {};
            out[0] = 0x24; out[1] = 2;                         // version 4, server; stratum 2
            std::memcpy(out + 24, in + 40, 8);                 // originate = client's transmit
            const qint64 now = QDateTime::currentMSecsSinceEpoch() + 1500;
            putTs(out, 32, now); putTs(out, 40, now);
            srv->writeDatagram(reinterpret_cast<char *>(out), 48, from, fp);
        }
    });
    th.start();
    const auto r = Ntp::query("127.0.0.1", port, 2000);
    CHECK(r.ok && qAbs(r.offsetMs - 1500) < 50);
    std::printf("     fake server: offset %lld ms, round trip %lld ms\n", r.offsetMs, r.roundTripMs);
    const auto dead = Ntp::query("127.0.0.1", 9, 500);      // nothing there: timeout, not a hang
    CHECK(!dead.ok && !dead.error.isEmpty());
    QMetaObject::invokeMethod(srv, [srv] { srv->deleteLater(); }, Qt::BlockingQueuedConnection);
    th.quit();
    th.wait();

    std::printf(failures ? "sun_ntp_test: %d FAILED\n" : "sun_ntp_test: all passed\n", failures);
    return failures ? 1 : 0;
}
