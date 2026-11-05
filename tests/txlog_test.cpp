// Checks TxLog (port of Logging.cs) - no database needed
#include "txlog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath("WSPR_Log");

    CHECK(TxLog::append(dir.path(), "WSPR_Log", "first"));
    CHECK(TxLog::append(dir.path() + "/", "WSPR_Log", "second"));   // trailing separator, as the C# slash handling
    QFile f(path);
    CHECK(f.open(QIODevice::ReadOnly | QIODevice::Text));
    CHECK(f.readAll() == "first\nsecond\n");                        // appended, one line each
    f.close();

    CHECK(!TxLog::append(dir.path() + "/missing", "WSPR_Log", "x"));   // C# also skipped a missing folder
    CHECK(!QFile::exists(dir.path() + "/missing/WSPR_Log"));

    const QDateTime start(QDate(2026, 11, 1), QTime(21, 42), QTimeZone::utc());
    const QString line = TxLog::transmissionLine(start, "G4GCI", 14.0956, "IO90GU", 23, "Windom");
    CHECK(line == "2026-11-01 21:42 UTC, G4GCI, 14.0956 MHz, IO90GU, 23 dBm, Antenna: Windom");
    CHECK(TxLog::transmissionLine(start, "G4GCI", 1296.5, "IO90", 0, "Dish").contains(", 1296.5 MHz,"));

    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
