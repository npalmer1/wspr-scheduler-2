// RxStore against a scratch MySQL/MariaDB loaded with tests/rx_fixture.sql
// (database wspr_rpt_selftest; user admin / wspr as the app default). Exit 77 = skipped (no server).
#include "rxstore.h"

#include <QCoreApplication>
#include <QTimeZone>
#include <cmath>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static RxSpot spot(const QString &line, int minute)
{
    RxSpot s = *Rx::parseWsprdLine(line, "IO90GU");
    s.datetime = QDateTime(QDate(2026, 11, 1), QTime(22, minute), QTimeZone::utc());
    s.reporter = "G4GCI";
    s.reporterLoc = "IO90GU";
    return s;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    RxStore st(DbSettings{}, "wspr_rpt_selftest");
    if (!st.count()) {
        std::printf("rxstore_test: no database (%s) - load tests/rx_fixture.sql first; skipped\n", qPrintable(st.lastError()));
        return 77;
    }
    CHECK(*st.count() == 0);

    const RxSpot a = spot(" 2250 -12 0.4 14.097152 1 K1ABC FN42 37", 50);
    const RxSpot b = spot(" 2250 -5 0.2 14.097122 0 <G4GCI/P> IO90GU 23", 50);
    const RxSpot c = spot(" 2252 3 -0.1 144.490512 0 G0XYZ IO91 30", 52);
    const RxSpot d = spot(" 2254 -20 0.9 0.137512 0 O'BRIEN JO01 40", 54);   // apostrophe, LF
    RxSpot nil;
    nil.txSign = Rx::kNilRcvd;
    nil.frequency = 7.0386;
    nil.band = 7;
    nil.datetime = QDateTime(QDate(2026, 11, 1), QTime(22, 56), QTimeZone::utc());
    nil.reporterLoc = "IO90GU";
    for (const RxSpot &s : {a, b, c, d, nil})
        CHECK(st.insert(s));
    CHECK(st.insert(a));                    // INSERT IGNORE: duplicate is not an error...
    CHECK(*st.count() == 5);                // ...and not stored twice

    auto all = st.latest(2000);
    CHECK(all && all->size() == 5);
    if (all && all->size() == 5) {
        CHECK((*all)[0].isNil() && (*all)[1].txSign == "O'BRIEN" && (*all)[3].txSign == "<G4GCI/P>");   // newest first, then lowest freq
        const RxSpot &r = (*all)[4];
        CHECK(r.txSign == "K1ABC" && r.txLoc == "FN42" && std::abs(r.frequency - 14.097152) < 1e-9 && r.power == 37
              && r.snr == -12 && r.drift == 1 && std::abs(r.dt - 0.4) < 1e-6 && r.reporter == "G4GCI"
              && r.datetime == a.datetime && r.distance == a.distance && r.azimuth == a.azimuth && r.band == 14);
        CHECK((*all)[2].band == 15);        // 2m stored with the fixed code
    }
    CHECK(st.latest(2)->size() == 2);

    RxFilter f;
    f.bandIndex = 9;                        // 20m: not 2m any more
    auto r = st.filtered(f, 2000);
    CHECK(r && r->size() == 2);
    f.bandIndex = 1;                        // LF: C# '0.136%' found nothing
    r = st.filtered(f, 2000);
    CHECK(r && r->size() == 1 && r->at(0).txSign == "O'BRIEN");
    f = {};
    f.call = "g4gci";                       // finds the type 3 "<G4GCI/P>"
    r = st.filtered(f, 2000);
    CHECK(r && r->size() == 1 && r->at(0).txSign == "<G4GCI/P>");
    f.call = "O'B*";                        // apostrophe: C# SQL broke here
    r = st.filtered(f, 2000);
    CHECK(r && r->size() == 1);
    f = {};
    f.minKm = 1000;
    r = st.filtered(f, 2000);
    CHECK(r && r->size() == 1 && r->at(0).txSign == "K1ABC");
    f = {};
    f.useDates = true;
    f.from = QDateTime(QDate(2026, 11, 1), QTime(22, 51), QTimeZone::utc());
    f.to = QDateTime(QDate(2026, 11, 1), QTime(22, 54), QTimeZone::utc());
    r = st.filtered(f, 2000);
    CHECK(r && r->size() == 2);
    f.bandIndex = 17;                       // dates + 2m
    r = st.filtered(f, 2000);
    CHECK(r && r->size() == 1 && r->at(0).txSign == "G0XYZ");

    CHECK(st.remove(c.datetime, c.txSign, 144.490512));   // matched to 0.5 Hz
    CHECK(*st.count() == 4);
    CHECK(st.remove(d.datetime, "O'BRIEN", d.frequency));
    CHECK(*st.count() == 3);

    CHECK(!st.readConfig().has_value());    // empty table
    CHECK(st.saveConfig({true, false, 3, true}));
    auto cfg = st.readConfig();
    CHECK(cfg && cfg->deep && !cfg->quick && cfg->osd == 3 && cfg->upload);
    CHECK(st.saveConfig({false, true, 0, false}));
    cfg = st.readConfig();
    CHECK(cfg && !cfg->deep && cfg->quick && cfg->osd == 0 && !cfg->upload);

    auto rs = st.readRxSettings();
    CHECK(rs && rs->inputName == "1: Microphone (3- USB Audio Devi" && rs->inLevel == 150
          && rs->wsprdPath == "C:/WSJT/wsjtx/bin" && !rs->sameDevice);

    RxSettings ns = *rs;                    // C#: Save_Audio, then read back
    ns.inputName = "Microphone (USB Audio Device)";
    ns.outputName = "Speakers (USB Audio Device)";
    ns.inLevel = 230;
    ns.wsprdPath = "C:\\WSJT\\wsjtx\\bin";
    ns.sameDevice = true;
    CHECK(st.saveRxSettings(ns));
    CHECK(st.saveRxSettings(ns));           // second save updates the same row
    rs = st.readRxSettings();
    CHECK(rs && rs->inputName == "Microphone (USB Audio Device)" && rs->inLevel == 230 && rs->sameDevice
          && rs->wsprdPath == "C:/WSJT/wsjtx/bin");

    RxStore bad(DbSettings{"127.0.0.1", 3306, "admin", "wrong"}, "wspr_rpt_selftest");
    CHECK(!bad.count() && !bad.insert(a) && !bad.latest(10) && !bad.lastError().isEmpty());

    std::printf(failures ? "rxstore_test: %d FAILED\n" : "rxstore_test: all passed\n", failures);
    return failures ? 1 : 0;
}
