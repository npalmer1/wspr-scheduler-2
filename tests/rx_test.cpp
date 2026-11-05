// Receive-side logic without a sound card or database: wsprd output, bands, comparison columns,
// cycle timing, wsprd arguments, wsprnet form, own-call rule, filter SQL, resampler and WAV writer.
#include "rxspot.h"
#include "rxstore.h"
#include "wavrecorder.h"
#include "wsprspectrum.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtEndian>
#include <cmath>
#include <cstdio>

static constexpr double kPi = 3.14159265358979323846;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static QDateTime utc(int y, int mo, int d, int h, int mi, int s, int ms = 0)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi, s, ms), QTimeZone::utc());
}

static void parseTests()
{
    // Real wsprd 2.x output lines (captured from wsprd run on wsprwav files)
    auto a = Rx::parseWsprdLine("  56  40  0.0  14.097052  0  G4GCI IO90 23 ", "IO91");
    CHECK(a && a->txSign == "G4GCI" && a->txLoc == "IO90" && a->power == 23 && a->snr == 40);
    CHECK(a && std::abs(a->frequency - 14.097052) < 1e-9 && a->band == 14 && a->drift == 0);
    CHECK(a && a->distance > 100 && a->distance < 120 && a->azimuth >= 179 && a->azimuth <= 180);   // 1 deg south

    auto b = Rx::parseWsprdLine("  56 -21  0.3  14.097082  -1  G4GCI/P 23 ", "IO90GU");
    CHECK(b && b->txSign == "G4GCI/P" && b->txLoc.isEmpty() && b->power == 23 && b->snr == -21);
    CHECK(b && b->distance == -1 && b->azimuth == -1 && b->drift == -1 && std::abs(b->dt - 0.3) < 1e-9);

    auto c = Rx::parseWsprdLine("  56  -5  0.0  14.097122  0  <G4GCI/P> IO90GU 23 ", "IO90GU");
    CHECK(c && c->txSign == "<G4GCI/P>" && c->txLoc == "IO90GU" && c->distance == 0 && c->azimuth == -1);

    CHECK(!Rx::parseWsprdLine("<DecodeFinished>", "IO90"));
    CHECK(!Rx::parseWsprdLine("", "IO90"));
    CHECK(!Rx::parseWsprdLine("garbage line here", "IO90"));
    auto bad = Rx::parseWsprdLine(" 0000 -10 0.1 7.040100 0 K1ABC FN42 xx", "IO90");   // C#: keep, drop loc
    CHECK(bad && bad->power == 0 && bad->txLoc.isEmpty() && bad->distance == -1);
    auto inv = Rx::parseWsprdLine(" 0000 -10 0.1 7.040100 0 K1ABC ZZ99 30", "IO90");  // invalid locator
    CHECK(inv && inv->distance == -1 && inv->azimuth == -1);

    const QDateTime cyc = utc(2026, 11, 1, 22, 50, 0);
    const QString out = "  56  40  0.0  14.097052  0  G4GCI IO90 23 \n"
                        "  56  40  0.0  14.097052  0  G4GCI IO90 23 \n"      // duplicate: dropped
                        "  56 -12  0.4  14.097152  1  K1ABC FN42 37 \n"
                        "<DecodeFinished>\n";
    auto r = Rx::parseWsprdOutput(out, cyc.addSecs(37), 14.0956, "G4GCI", "IO90GU");
    CHECK(r.finished && r.spots.size() == 2);
    CHECK(r.spots.size() == 2 && r.spots[1].txSign == "K1ABC" && r.spots[1].reporter == "G4GCI"
          && r.spots[1].reporterLoc == "IO90GU" && r.spots[1].datetime == cyc);

    auto nil = Rx::parseWsprdOutput("<DecodeFinished>\n", cyc, 7.0386, "G4GCI", "IO90GU");
    CHECK(nil.finished && nil.spots.size() == 1 && nil.spots[0].isNil());
    CHECK(nil.spots.size() == 1 && nil.spots[0].band == 7 && std::abs(nil.spots[0].frequency - 7.0386) < 1e-9
          && nil.spots[0].reporter.isEmpty() && nil.spots[0].reporterLoc == "IO90GU" && nil.spots[0].distance == 0);

    auto failed = Rx::parseWsprdOutput("  56  40  0.0  14.097052  0  G4GCI IO90 23 \n", cyc, 14.0956, "G4GCI", "IO90GU");
    CHECK(!failed.finished && failed.spots.isEmpty());   // no <DecodeFinished>: wsprd died
    CHECK(!Rx::parseWsprdOutput("", cyc, 14.0956, "G4GCI", "IO90").finished);
}

static void bandTests()
{
    struct { double f; int code; } t[] = {
        {0.137500, -1}, {0.475700, 0}, {1.838100, 1}, {3.570100, 3}, {5.288700, 5}, {5.366200, 5},
        {7.040100, 7}, {10.140200, 10}, {13.555400, 13}, {14.097100, 14}, {18.106100, 18}, {21.096100, 21},
        {24.926100, 24}, {28.126100, 28}, {40.681500, 40}, {50.294500, 50}, {70.092500, 70},
        {144.490500, 15},   // C# gave 14 (tested "14" before "144")
        {432.301500, 16}, {1296.501500, 17}, {99.0, -2}, {0.0, -2}};
    for (auto &x : t) {
        if (Rx::bandCode(x.f) != x.code)
            std::printf("  band %f -> %d, want %d\n", x.f, Rx::bandCode(x.f), x.code);
        CHECK(Rx::bandCode(x.f) == x.code);
    }
    const auto &br = Rx::bandRanges();
    CHECK(br.size() == 20 && br[0].name == "All" && br[1].name == "LF" && br[9].name == "20m" && br[17].name == "2m"
          && br[19].name == "23cm");
    CHECK(br[1].lo <= 0.1375 && 0.1375 < br[1].hi);   // C# filtered LF with '0.136%' and found nothing
    CHECK(!(br[9].lo <= 144.49 && 144.49 < br[9].hi));
}

static void columnTests()
{
    CHECK(Rx::wattsText(23) == "0.2" && Rx::wattsText(37) == "5" && Rx::wattsText(60) == "1000" && Rx::wattsText(22).isEmpty());
    CHECK(Rx::wattsToDbm(100) == 50 && Rx::wattsToDbm(5) == 36 && Rx::wattsToDbm(1500) == 61 && Rx::wattsToDbm(1000) == 60);
    // getCW thresholds, compare 100 W (50 dBm) with a 23 dBm spot: level = snr + 27
    CHECK(Rx::cwLabel(-38, 23, 50) == "unusable");    // -11
    CHECK(Rx::cwLabel(-37, 23, 50) == "very weak");   // -10
    CHECK(Rx::cwLabel(-33, 23, 50) == "copyable");    // -6
    CHECK(Rx::cwLabel(-29, 23, 50) == "readable");    // -2
    CHECK(Rx::cwLabel(-24, 23, 50) == "solid");       // 3
    CHECK(Rx::cwLabel(-19, 23, 50) == "very good");   // 8
    CHECK(Rx::cwLabel(-14, 23, 50) == "strong");      // 13
    CHECK(Rx::ssbLabel(-23, 23, 50) == "unusable");   // 4
    CHECK(Rx::ssbLabel(-22, 23, 50) == "borderline"); // 5
    CHECK(Rx::ssbLabel(-17, 23, 50) == "workable");   // 10
    CHECK(Rx::ssbLabel(-15, 23, 50) == "good");       // 12
    CHECK(Rx::ssbLabel(-12, 23, 50) == "excellent");  // 15
    CHECK(Rx::kmToMiles(100) == 62 && Rx::kmToMiles(20035) == 12449 && Rx::kmToMiles(20000) == 12427);
    CHECK(Rx::milesToKm(100) == 160 && Rx::milesToKm(1) == 1);
    CHECK(Rx::snrText(3) == "+3" && Rx::snrText(0) == "0" && Rx::snrText(-21) == "-21");
}

static void timingTests()
{
    CHECK(Rx::recordDurationMs(utc(2026, 11, 1, 22, 50, 0)) == 110000);
    CHECK(Rx::recordDurationMs(utc(2026, 11, 1, 22, 49, 59)) == 111000);
    CHECK(Rx::recordDurationMs(utc(2026, 11, 1, 22, 50, 1)) == 109000);
    CHECK(Rx::recordDurationMs(utc(2026, 11, 1, 22, 50, 1, 400)) == 108600);
    CHECK(Rx::cycleStart(utc(2026, 11, 1, 22, 49, 59)) == utc(2026, 11, 1, 22, 50, 0));
    CHECK(Rx::cycleStart(utc(2026, 11, 1, 22, 50, 1)) == utc(2026, 11, 1, 22, 50, 0));
    CHECK(Rx::cycleStart(utc(2026, 11, 1, 23, 59, 59)) == utc(2026, 11, 2, 0, 0, 0));   // midnight
    CHECK(Rx::cycleStart(utc(2026, 11, 1, 22, 51, 30)) == utc(2026, 11, 1, 22, 50, 0));
}

static void wsprdAndUploadTests()
{
    const QStringList a = Rx::wsprdArguments("C:/WSPR Sked", "14.095600", true, true, 3, "C:/WSPR Sked/temp1.wav");
#ifdef Q_OS_WIN
    CHECK(a == (QStringList{"-a", "C:\\WSPR Sked", "-f", "14.095600", "-d", "-o", "3", "C:\\WSPR Sked\\temp1.wav"}));
#else
    CHECK(a == (QStringList{"-a", "C:/WSPR Sked", "-f", "14.095600", "-d", "-o", "3", "C:/WSPR Sked/temp1.wav"}));
#endif
    CHECK(Rx::wsprdArguments("d", "7.0386", false, true, 0, "w.wav") == (QStringList{"-a", "d", "-f", "7.0386", "-q", "w.wav"}));
    CHECK(Rx::wsprdArguments("d", "7.0386", false, false, 9, "w.wav").contains("5"));

    RxSpot s = *Rx::parseWsprdLine("  56 -12  0.4  14.097152  1  K1ABC FN42 37 ", "IO90GU");
    s.datetime = utc(2026, 11, 1, 22, 50, 0);
    const auto f = Rx::wsprnetForm(s, "WS-0.2.1", "G4GCI", "IO90GU", "14.095600");
    QStringList flat;
    for (const auto &kv : f)
        flat << kv.first + '=' + kv.second;
    CHECK(flat == (QStringList{"function=wspr", "date=261101", "time=2250", "sig=-12", "dt=0.4", "drift=1",
                               "tqrg=14.097152", "tcall=K1ABC", "tgrid=FN42", "dbm=37", "version=WS-0.2.1",
                               "rcall=G4GCI", "rgrid=IO90GU", "rqrg=14.095600", "mode=2"}));

    CHECK(Rx::isOwnCall("G4GCI", "G4GCI") && Rx::isOwnCall("<G4GCI/P>", "G4GCI/P") && Rx::isOwnCall("G4GCI", "G4GCI/P")
          && Rx::isOwnCall("g4gci/p", "G4GCI") && !Rx::isOwnCall("K1ABC", "G4GCI") && !Rx::isOwnCall("<...>", "G4GCI"));
    CHECK(Rx::shouldUpload(s, "G4GCI", false));
    RxSpot own = s; own.txSign = "<G4GCI/P>";
    CHECK(!Rx::shouldUpload(own, "G4GCI/P", false) && Rx::shouldUpload(own, "G4GCI/P", true));
    RxSpot nil = s; nil.txSign = Rx::kNilRcvd;
    CHECK(!Rx::shouldUpload(nil, "G4GCI", true));
}

static void wsprdLookupTests()
{
#ifdef Q_OS_WIN
    const QString name = "wsprd.exe";
#else
    const QString name = "wsprd";
#endif
    const QDir app(QCoreApplication::applicationDirPath());
    const bool existed = app.exists("wsprd");
    app.mkpath("wsprd");
    QFile f(app.filePath("wsprd/" + name));
    f.open(QIODevice::WriteOnly);
    f.close();
    CHECK(Rx::wsprdExecutable("") == QDir::toNativeSeparators(app.filePath("wsprd/" + name)));
    CHECK(Rx::wsprdExecutable("/no/such/folder") == QDir::toNativeSeparators(app.filePath("wsprd/" + name)));
    QTemporaryDir own;
    QFile g(own.filePath(name));
    g.open(QIODevice::WriteOnly);
    g.close();
    CHECK(Rx::wsprdExecutable(own.path()) == QDir::toNativeSeparators(own.filePath(name)));   // configured folder first
    CHECK(Rx::wsprdExecutable(own.filePath(name)) == QDir::toNativeSeparators(own.filePath(name)));   // or the file itself
    f.remove();
    if (!existed)
        app.rmdir("wsprd");
}

static void filterSqlTests()
{
    RxFilter f;
    auto q = RxStore::filterQuery(f, 2000);
    CHECK(q.sql == "SELECT * FROM received ORDER BY datetime DESC, frequency ASC LIMIT 2000" && q.binds.isEmpty());
    f.useDates = true;
    f.from = utc(2026, 11, 1, 0, 0, 0);
    f.to = utc(2026, 11, 1, 23, 59, 0);
    f.bandIndex = 9;   // 20m
    f.call = "o'b*_";
    f.minKm = 100;
    f.maxKm = 5000;
    q = RxStore::filterQuery(f, 50);
    CHECK(q.sql == "SELECT * FROM received WHERE datetime >= ? AND datetime <= ? AND frequency >= ? AND frequency < ? "
                   "AND (tx_sign LIKE ? OR tx_sign LIKE ?) AND distance >= ? AND distance <= ? "
                   "ORDER BY datetime DESC, frequency ASC LIMIT 50");
    CHECK(q.binds.size() == 8 && q.binds[0] == "2026-11-01 00:00:00" && q.binds[2].toDouble() == 14.0
          && q.binds[4] == "O'B\\_%" && q.binds[5] == "<O'B\\_%" && q.binds[6] == 100);
}

static double toneAmplitude(const QVector<float> &x, double hz, int rate, int from, int n)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i) {
        const double ph = 2 * kPi * hz * (from + i) / rate;
        re += x[from + i] * std::cos(ph);
        im -= x[from + i] * std::sin(ph);
    }
    return 2 * std::sqrt(re * re + im * im) / n;
}

static void resamplerTests()
{
    for (int rate : {12000, 44100, 48000, 96000}) {
        // 1 s of 1500 Hz at 0.5 plus 7 kHz at 0.5 (must not fold back into the 12 kHz result)
        QVector<float> in(rate);
        for (int i = 0; i < rate; ++i)
            in[i] = float(0.5 * std::sin(2 * kPi * 1500.0 * i / rate)
                          + (rate > 14000 ? 0.5 * std::sin(2 * kPi * 7000.0 * i / rate) : 0.0));
        Resampler12k rs(rate);
        QVector<float> out;
        for (int i = 0; i < rate; i += 1000)   // in odd-sized blocks, as a sound card delivers
            rs.process(in.constData() + i, qMin(1000, rate - i), out);
        const bool sizeOk = std::abs(out.size() - 12000) < 200;
        const double a1500 = toneAmplitude(out, 1500, 12000, 1000, 9000);
        const double alias = toneAmplitude(out, 12000 - 7000, 12000, 1000, 9000);   // 7 kHz would land on 5 kHz
        if (!sizeOk || std::abs(a1500 - 0.5) > 0.01 || alias > 0.005)
            std::printf("  rate %d: %d samples, 1500 Hz %.4f, alias %.5f\n", rate, int(out.size()), a1500, alias);
        CHECK(sizeOk);
        CHECK(std::abs(a1500 - 0.5) < 0.01);
        CHECK(alias < 0.005);
    }
}

static void wavTests()
{
    QTemporaryDir dir;
    const QString p = dir.filePath("t.wav");
    WavWriter w;
    CHECK(w.open(p));
    const float s[] = {0.0f, 0.5f, -0.5f, 1.5f, -2.0f};
    w.write(s, 5, 2.0f);   // gain 2 and clipping, as C#
    CHECK(w.close());
    QFile f(p);
    CHECK(f.open(QIODevice::ReadOnly));
    const QByteArray b = f.readAll();
    CHECK(b.size() == 44 + 10 && b.startsWith("RIFF") && b.mid(8, 8) == "WAVEfmt " && b.mid(36, 4) == "data");
    CHECK(qFromLittleEndian<quint32>(b.constData() + 4) == 46 && qFromLittleEndian<quint32>(b.constData() + 24) == 12000
          && qFromLittleEndian<quint32>(b.constData() + 40) == 10);
    const qint16 *pcm = reinterpret_cast<const qint16 *>(b.constData() + 44);
    CHECK(pcm[0] == 0 && pcm[1] == 32767 && pcm[2] == -32767 && pcm[3] == 32767 && pcm[4] == -32767);
}

static void spectrumTests()
{
    // 1500 Hz at 0.1 plus a quieter 1400 Hz: peaks in the right bins, colour scale as C#
    QVector<float> x(Spectrum::kFftSize);
    for (int i = 0; i < x.size(); ++i)
        x[i] = float(0.1 * std::sin(2 * kPi * 1500.0 * i / 12000) + 0.001 * std::sin(2 * kPi * 1400.0 * i / 12000));
    const QVector<float> db = Spectrum::lineDb(x.constData(), 0.0f);
    CHECK(db.size() == 547);
    const double res = 12000.0 / 16384;
    auto hz = [&](int i) { return (int(1300 / res) + i) * res; };
    int best = 0;
    for (int i = 1; i < db.size(); ++i)
        if (db[i] > db[best]) best = i;
    CHECK(std::abs(hz(best) - 1500) < 1.0);
    CHECK(std::abs(db[best] - float(20 * std::log10(0.1 / 4))) < 1.0);   // Hann: amplitude/4 after 1/N scaling
    int i1400 = int(std::lround(1400 / res)) - int(1300 / res);
    CHECK(db[i1400] < db[best] - 30 && db[i1400] > db[best] - 50);
    CHECK(Spectrum::lineDb(x.constData(), 10.0f)[best] - db[best] > 9.99f);   // gain slider adds dB
    CHECK(Spectrum::colour(-100) == 0x000000 && Spectrum::colour(-20) == 0xff0000 && Spectrum::colour(-50) == 0x00ff7f);
    int a, b;
    Spectrum::binsForPixel(0, 100, 547, &a, &b);
    CHECK(a == 0 && b == 5);
    Spectrum::binsForPixel(999, 1000, 547, &a, &b);
    CHECK(a == 546 && b == 547);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    parseTests();
    bandTests();
    columnTests();
    timingTests();
    wsprdAndUploadTests();
    filterSqlTests();
    wsprdLookupTests();
    resamplerTests();
    wavTests();
    spectrumTests();
    std::printf(failures ? "rx_test: %d FAILED\n" : "rx_test: all passed\n", failures);
    return failures ? 1 : 0;
}
