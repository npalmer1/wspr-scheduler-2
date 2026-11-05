// Checks SlotStore and the repeat planner against a scratch MySQL/MariaDB loaded with tests/fixture.sql.
// Build with -DWSPRSKED_TESTS=ON. Uses the slot database "wspr_slots_selftest", which it drops and recreates.
#include "repeatplanner.h"
#include "slotstore.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static QStringList fmt(const SlotTimes &t)
{
    QStringList s;
    for (const auto &p : t)
        s << p.first.toString("MM-dd") + ' ' + p.second.toString("HH:mm");
    return s;
}

static void plannerTests()
{
    const QDate d(2026, 11, 1);
    RepeatRequest r;
    r.startDate = d;
    r.slotTime = QTime(10, 20);
    r.endDate = d.addDays(2);

    CHECK(fmt(planSlotTimes(r, {})) == QStringList{"11-01 10:20"});            // not repeating

    r.repeat = true;
    r.rptType = 0;                                                             // same time daily
    CHECK(fmt(planSlotTimes(r, {})) == (QStringList{"11-01 10:20", "11-02 10:20", "11-03 10:20"}));

    r.thisSlotOnly = true;
    CHECK(planSlotTimes(r, {}).size() == 1);
    r.thisSlotOnly = false;

    r.rptType = 1;                                                             // hourly until 12:20 (exclusive, as C#)
    r.endTime = QTime(12, 20);
    r.endDate = d;
    CHECK(fmt(planSlotTimes(r, {})) == (QStringList{"11-01 10:20", "11-01 11:20"}));
    r.endTime = QTime(12, 30);
    CHECK(fmt(planSlotTimes(r, {})) == (QStringList{"11-01 10:20", "11-01 11:20", "11-01 12:20"}));
    QString err;
    r.endTime = QTime(9, 0);
    CHECK(planSlotTimes(r, {}, &err).isEmpty() && !err.isEmpty());

    r.rptType = 4;                                                             // every hour, all day
    auto all = planSlotTimes(r, {});
    CHECK(all.size() == 24 && all.first().second == QTime(0, 20) && all.last().second == QTime(23, 20));

    // Day / night with UTC sun times 06:56 / 16:37
    SunLookup sun = [](const QDate &) { return std::optional<SunWindow>(SunWindow{QTime(6, 56), QTime(16, 37)}); };
    r.rptType = 2;
    r.greyOffset = 0;
    auto day = planSlotTimes(r, sun);
    CHECK(day.first().second == QTime(6, 20) && day.last().second == QTime(16, 20) && day.size() == 11);
    r.greyOffset = 60;                                                         // window 05:56 - 17:37(+38)
    day = planSlotTimes(r, sun);
    CHECK(day.first().second == QTime(5, 20) && day.last().second == QTime(17, 20));

    r.rptType = 3;
    r.greyOffset = 0;
    auto night = planSlotTimes(r, sun);
    QStringList n = fmt(night);
    CHECK(n.first() == "11-01 00:20" && n.contains("11-01 06:20") && !n.contains("11-01 07:20"));
    CHECK(n.contains("11-01 16:20") && n.last() == "11-01 23:20" && !n.contains("11-01 15:20"));

    SunLookup none = [](const QDate &) { return std::optional<SunWindow>(); };
    CHECK(planSlotTimes(r, none, &err).isEmpty());
}

static void storeTests()
{
    SlotStore store;
    CHECK(store.databaseExists("wspr"));
    const QString db = "wspr_slots_selftest";
    {
        QSqlDatabase d = QSqlDatabase::addDatabase("QMYSQL", "drop");
        d.setHostName("127.0.0.1"); d.setUserName("admin"); d.setPassword("wspr");
        CHECK(d.open());
        d.exec("DROP DATABASE IF EXISTS " + db);
        d.close();
    }
    QSqlDatabase::removeDatabase("drop");

    CHECK(store.createSlotDatabase(db));
    store.setSlotDbName(db);
    store.ensureIndexes();
    CHECK(store.slotDbReachable());
    CHECK(store.hasSwitch2Columns());

    auto cfg = store.loadConfig();
    CHECK(cfg && cfg->callsign == "G4GCI" && cfg->locator == "IO90GU" && cfg->defaultdBm == 23);
    auto ants = store.loadAntennas();
    CHECK(ants.size() == 3 && ants[1].swPort == 2);
    auto sun = store.sunTimes("IO90", QDate(2026, 6, 21));
    CHECK(sun && sun->rise == QTime(4, 59) && sun->set == QTime(21, 22));
    CHECK(store.allSunTimes("IO90").size() == 4);

    SlotRecord r;
    r.date = QDate(2026, 11, 1);
    r.time = QTime(10, 20);
    r.freq = 14.0956;
    r.offset = 50;
    r.powerdBm = 23;
    r.powerW = 0.2;
    r.antenna = "G4GCI's loop";   // apostrophe broke the C# UPDATE clause
    r.endDate = r.date;
    r.endTime = r.time;
    r.active = true;
    r.parent = "2026-11-01 10:20";
    r.sw2 = 3;
    r.swPort2 = 4;
    CHECK(store.upsert(r));
    auto hour = store.loadHour(r.date, 10);
    CHECK(hour && hour->size() == 1);
    const SlotRecord back = hour->value(QTime(10, 20));
    CHECK(back.antenna == r.antenna && back.active && back.powerW == 0.2 && back.sw2 == 3 && back.swPort2 == 4);

    // repeat set: 3 days x 24 h in one transaction, then delete by parent
    RepeatRequest rq;
    rq.startDate = r.date; rq.slotTime = QTime(10, 30); rq.repeat = true; rq.rptType = 4; rq.endDate = r.date.addDays(2);
    QVector<SlotRecord> rows;
    for (const auto &[d, t] : planSlotTimes(rq, {})) {
        SlotRecord x = r;
        x.date = d; x.time = t; x.repeating = true; x.rptType = 4; x.parent = "2026-11-0100:00";
        rows << x;
    }
    CHECK(rows.size() == 72);
    CHECK(store.upsertMany(rows));
    CHECK(store.loadHour(r.date.addDays(2), 23)->size() == 1);
    r.freq = 7.0386;                                                   // update in place
    CHECK(store.upsert(r));
    CHECK(store.find(r.date, r.time)->freq == 7.0386);
    CHECK(store.deleteByParent("2026-11-0100:00"));
    CHECK(store.loadHour(r.date, 10)->size() == 1);                    // only the single slot left
    CHECK(!store.deleteByParent(""));                                  // refuses to wipe parent-less slots
    CHECK(store.deleteOne(r.date, r.time));
    CHECK(store.loadHour(r.date, 10)->isEmpty());

    CHECK(store.upsertMany(rows));
    CHECK(store.deleteBetween(r.date.addDays(1), r.date.addDays(1)));
    CHECK(store.loadHour(r.date, 5)->size() == 1 && store.loadHour(r.date.addDays(1), 5)->isEmpty());
    CHECK(store.deleteBefore(r.date.addDays(3)));
    CHECK(store.loadHour(r.date.addDays(2), 5)->isEmpty());

    CHECK(store.saveSlotDbName(1, "wspr_slots"));

    // bad credentials -> reported as an error, no crash
    SlotStore bad(DbSettings{"127.0.0.1", 3306, "admin", "wrong"});
    CHECK(!bad.slotDbReachable());
    CHECK(!bad.loadHour(r.date, 10).has_value());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    plannerTests();
    if (!SlotStore().databaseExists("wspr")) {
        std::printf("database not reachable - load tests/fixture.sql into a scratch server; store tests skipped\n");
        return failures ? 1 : 77;
    }
    storeTests();
    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
