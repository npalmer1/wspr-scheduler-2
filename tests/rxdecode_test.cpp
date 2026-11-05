// Receive path without a sound card: a WSPR signal plus noise at a sound card's own rate
// (48 / 44.1 / 12 kHz) goes through the same resampler + WAV writer the recorder uses, cut off at
// second 110 like a real recording, then RxCycle runs the real wsprd on it.
// Also: type 2 -> type 3 hash resolution in the data folder, blockDecodes, and the wsprnet upload
// against a local fake server (form body, timeout).
// Needs wsprd on PATH (or WSPRD_DIR=folder).
#include "rxcycle.h"
#include "wavrecorder.h"
#include "wsprencode.h"
#include "wsprmodulator.h"
#include "wsprnetupload.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>
#include <cmath>
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

// Writes what WsprAudioIn would write for this signal arriving at `rate`
static bool makeRecording(const QString &path, const QString &msg, int offsetHz, int rate, double noise)
{
    QString err;
    const auto sym = WsprEncode::encodeMessage(msg, &err);
    if (!sym) {
        std::printf("encode %s: %s\n", qPrintable(msg), qPrintable(err));
        return false;
    }
    WsprModulator::Params p;
    p.offsetHz = offsetHz;
    p.sampleRate = rate;
    WsprModulator mod(*sym, p);
    const QVector<float> sig = mod.renderAll();

    std::mt19937 gen(1234 + rate);
    std::normal_distribution<float> nd(0.0f, float(noise));
    const int lead = rate;                       // transmission starts at second 1
    const qint64 total = qint64(rate) * 110;     // recording stops at second 110 (cuts the last 1.6 s)
    QVector<float> in(total);
    for (qint64 i = 0; i < total; ++i) {
        const qint64 k = i - lead;
        in[i] = (k >= 0 && k < sig.size() ? 0.1f * sig[k] : 0.0f) + nd(gen);
    }
    Resampler12k rs(rate);
    WavWriter w;
    if (!w.open(path))
        return false;
    QVector<float> out;
    const int block = rate / 50;                 // 20 ms blocks, as a sound card delivers
    for (qint64 i = 0; i < total; i += block) {
        out.clear();
        rs.process(in.constData() + i, int(qMin<qint64>(block, total - i)), out);
        w.write(out.constData(), out.size(), 1.0f);
    }
    return w.close();
}

struct Result { bool got = false; Rx::DecodeResult r; QString status; QString problem; };

static Result decode(RxCycle &rx, const QString &wav, const QString &dial)
{
    Result res;
    QEventLoop loop;
    auto c1 = QObject::connect(&rx, &RxCycle::decoded, [&](const Rx::DecodeResult &r, const QString &, const QDateTime &) {
        res.got = true;
        res.r = r;
    });
    auto c2 = QObject::connect(&rx, &RxCycle::statusChanged, [&](const QString &s) {
        res.status = s;
        if (s != "decoding")
            loop.quit();
    });
    auto c3 = QObject::connect(&rx, &RxCycle::problem, [&](const QString &m) { res.problem = m; loop.quit(); });
    QTimer::singleShot(120000, &loop, &QEventLoop::quit);
    const QDateTime cyc(QDate(2026, 11, 1), QTime(22, 50), QTimeZone::utc());
    if (rx.decodeFile(wav, cyc, dial))
        loop.exec();
    QObject::disconnect(c1);
    QObject::disconnect(c2);
    QObject::disconnect(c3);
    return res;
}

static const RxSpot *find(const Rx::DecodeResult &r, const QString &call)
{
    for (const RxSpot &s : r.spots)
        if (s.txSign == call)
            return &s;
    return nullptr;
}

static void decodeTests()
{
    if (Rx::wsprdExecutable(qEnvironmentVariable("WSPRD_DIR")).isEmpty()) {
        std::printf("rxdecode_test: wsprd not found, decode tests skipped\n");
        return;
    }
    QTemporaryDir dir;
    RxCycle rx;
    rx.setDataDir(dir.path());
    rx.setWsprdDir(qEnvironmentVariable("WSPRD_DIR"));
    rx.setReporter("G4GCI", "IO90GU");

    // 1. type 1 recorded at 48 kHz
    const QString w1 = dir.filePath("temp1.wav");
    CHECK(makeRecording(w1, "K1ABC FN42 37", 50, 48000, 0.3));
    Result r = decode(rx, w1, "14.095600");
    const RxSpot *s = find(r.r, "K1ABC");
    CHECK(r.got && r.r.finished && s);
    if (s) {
        std::printf("  48 kHz: %s %s %d dBm snr %d dt %.1f f %.6f, %d km az %d\n", qPrintable(s->txSign),
                    qPrintable(s->txLoc), s->power, s->snr, s->dt, s->frequency, s->distance, s->azimuth);
        CHECK(s->txLoc == "FN42" && s->power == 37 && s->band == 14);
        CHECK(std::abs(s->frequency - (14.0956 + 1450.0 / 1e6 + 1.5 * 1.4648 / 1e6)) < 3e-6);   // centre of the 4 tones
        CHECK(s->distance > 5000 && s->distance < 5300 && s->azimuth > 280 && s->azimuth < 300);
        CHECK(s->datetime == QDateTime(QDate(2026, 11, 1), QTime(22, 50), QTimeZone::utc()) && s->reporter == "G4GCI");
        CHECK(std::abs(s->dt) < 1.0);   // ~0 when the transmission starts at second 1
    }

    // 2. type 2 at 44.1 kHz, then type 3 at 12 kHz: the hash from (2) resolves (3)'s call
    const QString w2 = dir.filePath("temp2.wav");
    CHECK(makeRecording(w2, "G4GCI/P 23", 120, 44100, 0.3));
    r = decode(rx, w2, "7.038600");
    s = find(r.r, "G4GCI/P");
    CHECK(s && s->txLoc.isEmpty() && s->power == 23 && s->band == 7 && s->distance == -1);
    const QString w3 = dir.filePath("temp3.wav");
    CHECK(makeRecording(w3, "<G4GCI/P> IO90GU 23", 120, 12000, 0.3));
    r = decode(rx, w3, "7.038600");
    s = find(r.r, "<G4GCI/P>");
    if (!s)
        for (const RxSpot &x : r.r.spots)
            std::printf("  type 3 got: %s %s\n", qPrintable(x.txSign), qPrintable(x.txLoc));
    CHECK(s && s->txLoc == "IO90GU" && s->distance == 0 && s->azimuth == -1);

    // 3. noise only -> one "nil rcvd" row at the dial frequency
    const QString w4 = dir.filePath("noise.wav");
    {
        WavWriter w;
        w.open(w4);
        std::mt19937 gen(7);
        std::normal_distribution<float> nd(0.0f, 0.3f);
        QVector<float> n(12000 * 110);
        for (float &x : n)
            x = nd(gen);
        w.write(n.constData(), n.size());
        w.close();
    }
    r = decode(rx, w4, "10.138700");
    CHECK(r.got && r.r.spots.size() == 1 && r.r.spots[0].isNil() && r.r.spots[0].band == 10);

    // 4. blockDecodes (set while transmitting): no decode
    rx.setBlockDecodes(true);
    r = decode(rx, w1, "14.095600");
    CHECK(!r.got && r.status == "idle/not receiving");
    rx.setBlockDecodes(false);

    // 5. deep search + OSD still decodes
    rx.setOptions({true, false, 2});
    r = decode(rx, w1, "14.095600");
    CHECK(r.got && find(r.r, "K1ABC"));
}

// Minimal HTTP server: answers every POST with "200 ok" (or never, for the timeout test)
class FakeWsprnet : public QTcpServer
{
public:
    QByteArray lastRequest;
    bool silent = false;
    FakeWsprnet()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [this, s] {
                    lastRequest += s->readAll();
                    const int hdrEnd = lastRequest.indexOf("\r\n\r\n");
                    if (hdrEnd < 0)
                        return;
                    const QByteArray hdr = lastRequest.left(hdrEnd).toLower();
                    const int cl = hdr.indexOf("content-length:");
                    const int len = cl < 0 ? 0 : hdr.mid(cl + 15, hdr.indexOf("\r\n", cl) - cl - 15).trimmed().toInt();
                    if (lastRequest.size() - hdrEnd - 4 < len || silent)
                        return;
                    s->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 15\r\nConnection: close\r\n\r\n"
                             "1 spot(s) added");
                    s->disconnectFromHost();
                });
            }
        });
    }
};

static void uploadTests()
{
    FakeWsprnet srv;
    CHECK(srv.listen(QHostAddress::LocalHost));
    WsprnetUpload up;
    up.setUrl(QUrl(QString("http://127.0.0.1:%1/post/").arg(srv.serverPort())));
    up.setUserAgent("WS-0.2.1");
    up.setTimeoutMs(2000);

    RxSpot s = *Rx::parseWsprdLine(" 2250 -5 0.2 14.097122 0 <G4GCI/P> IO90GU 23", "IO91");
    s.datetime = QDateTime(QDate(2026, 11, 1), QTime(22, 50), QTimeZone::utc());
    int status = -1;
    QString body;
    QEventLoop loop;
    QObject::connect(&up, &WsprnetUpload::finished, [&](const QString &, int st, const QString &b) {
        status = st;
        body = b;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    up.post(Rx::wsprnetForm(s, "WS-0.2.1", "G4GCI", "IO91", "14.095600"), s.txSign);
    loop.exec();
    CHECK(status == 200 && body == "1 spot(s) added");
    const QByteArray req = srv.lastRequest;
    CHECK(req.startsWith("POST /post/ HTTP/1.1"));
    CHECK(req.toLower().contains("content-type: application/x-www-form-urlencoded"));
    CHECK(req.contains("User-Agent: WS-0.2.1"));
    const QUrlQuery form(QString::fromUtf8(req.mid(req.indexOf("\r\n\r\n") + 4)));
    CHECK(form.queryItemValue("tcall", QUrl::FullyDecoded) == "<G4GCI/P>");
    CHECK(form.queryItemValue("tgrid") == "IO90GU" && form.queryItemValue("date") == "261101"
          && form.queryItemValue("time") == "2250" && form.queryItemValue("sig") == "-5"
          && form.queryItemValue("rqrg") == "14.095600" && form.queryItemValue("mode") == "2");
    if (!form.hasQueryItem("tcall"))
        std::printf("request was:\n%s\n", req.constData());

    // a server that never answers: finished with status 0 after the timeout, not a hang
    srv.silent = true;
    srv.lastRequest.clear();
    up.setTimeoutMs(500);
    status = -1;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    up.post(Rx::wsprnetForm(s, "WS-0.2.1", "G4GCI", "IO91", "14.095600"), s.txSign);
    loop.exec();
    CHECK(status == 0 && up.pending() == 0);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Rx::setDebugLogPath(QDir::temp().filePath("rxdecode_test_debug.txt"));
    uploadTests();
    decodeTests();
    std::printf(failures ? "rxdecode_test: %d FAILED\n" : "rxdecode_test: all passed\n", failures);
    return failures ? 1 : 0;
}
