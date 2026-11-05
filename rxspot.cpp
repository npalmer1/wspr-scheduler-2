#include "rxspot.h"

#include "maidenhead.h"
#include "slotrecord.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimeZone>
#include <cmath>

const QString Rx::kNilRcvd = QStringLiteral("nil rcvd");

bool RxSpot::isNil() const { return txSign == Rx::kNilRcvd; }

namespace Rx {

// ---- wsprd output -------------------------------------------------------------------------

std::optional<RxSpot> parseWsprdLine(const QString &line, const QString &myLoc)
{
    const QStringList R = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    // time snr dt freq drift CALL [LOC] dBm
    if (R.size() != 7 && R.size() != 8)
        return std::nullopt;
    bool ok = true, all = true;
    RxSpot s;
    s.snr = R[1].toInt(&ok);            all &= ok;
    s.dt = R[2].toDouble(&ok);          all &= ok;
    const double f = R[3].toDouble(&ok); all &= ok;
    s.drift = R[4].toInt(&ok);          all &= ok;
    if (!all || f <= 0)
        return std::nullopt;
    s.frequency = std::round(f * 1e6) / 1e6;
    s.band = bandCode(s.frequency);
    s.txSign = R[5];
    const bool hasLoc = R.size() == 8;   // type 2 messages have no locator
    s.txLoc = hasLoc ? R[6] : QString();
    s.power = R[hasLoc ? 7 : 6].toInt(&ok);
    if (!ok) {                           // as C#: keep the decode, drop the locator, power 0
        s.power = 0;
        s.txLoc.clear();
    }

    s.distance = -1;
    s.azimuth = -1;
    if (!s.txLoc.trimmed().isEmpty()) {
        const auto km = Maidenhead::distanceKm(myLoc, s.txLoc);
        const auto az = Maidenhead::azimuthDeg(myLoc, s.txLoc);
        if (km && az) {                  // C# measured to (91,181) for an invalid locator
            s.distance = int(*km);
            s.azimuth = s.distance == 0 ? -1 : int(*az);
        }
    }
    return s;
}

DecodeResult parseWsprdOutput(const QString &output, const QDateTime &cycleStart, double dialMHz,
                              const QString &reporter, const QString &myLoc)
{
    DecodeResult r;
    QSet<QString> seen;
    const QStringList lines = output.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.contains("<DecodeFinished>")) {
            r.finished = true;
            break;
        }
        auto s = parseWsprdLine(line, myLoc);
        if (!s) {
            ++r.skippedLines;
            continue;
        }
        // wsprd sometimes prints the same decode twice: drop exact repeats (as C#)
        const QString key = QString("%1|%2|%3|%4|%5").arg(s->txSign, s->txLoc)
                                .arg(s->frequency, 0, 'f', 6).arg(s->snr).arg(s->drift);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        r.spots.append(*s);
    }
    if (!r.finished) {
        r.spots.clear();
        return r;
    }
    if (r.spots.isEmpty()) {
        RxSpot nil;
        nil.txSign = kNilRcvd;
        nil.frequency = std::round(dialMHz * 1e6) / 1e6;
        nil.band = bandCode(nil.frequency);
        nil.distance = 0;     // as C#
        nil.azimuth = 0;
        r.spots.append(nil);
    }
    const QDateTime t = QDateTime(cycleStart.date(), QTime(cycleStart.time().hour(), cycleStart.time().minute()),
                                  QTimeZone::utc());
    for (RxSpot &s : r.spots) {
        s.datetime = t;
        s.reporter = s.isNil() ? QString() : reporter;   // C# left reporter empty on nil rows
        s.reporterLoc = myLoc;                            // C# always saved my_loc here
    }
    return r;
}

// ---- bands --------------------------------------------------------------------------------

namespace {
struct BandDef { const char *name; double lo, hi; int code; };
// Ranges cover the C# prefixes ("14" -> 14.x, "7." -> 7.x ...) without their overlaps.
const BandDef kBands[] = {
    {"LF",    0.13,    0.14,   -1},
    {"MF",    0.47,    0.48,    0},
    {"160m",  1.8,     2.0,     1},
    {"80m",   3.5,     4.0,     3},
    {"60m",   5.0,     6.0,     5},
    {"40m",   7.0,     8.0,     7},
    {"30m",  10.0,    11.0,    10},
    {"22m",  13.0,    14.0,    13},
    {"20m",  14.0,    15.0,    14},
    {"17m",  18.0,    19.0,    18},
    {"15m",  21.0,    22.0,    21},
    {"12m",  24.0,    25.0,    24},
    {"10m",  28.0,    30.0,    28},
    {"8m",   40.0,    41.0,    40},
    {"6m",   50.0,    55.0,    50},
    {"4m",   70.0,    72.0,    70},
    {"2m",  144.0,   149.0,    15},
    {"70cm", 430.0,  441.0,    16},
    {"23cm", 1240.0, 1301.0,   17},
};
} // namespace

int bandCode(double mhz)
{
    for (const BandDef &b : kBands)
        if (mhz >= b.lo && mhz < b.hi)
            return b.code;
    return -2;
}

const QVector<BandRange> &bandRanges()
{
    static const QVector<BandRange> v = [] {
        QVector<BandRange> r{{"All", 0, 0}};
        for (const BandDef &b : kBands)
            r.append({QString::fromLatin1(b.name), b.lo, b.hi});
        return r;
    }();
    return v;
}

QString bandMetres(double mhz)
{
    for (const BandDef &b : kBands)
        if (mhz >= b.lo && mhz < b.hi) {
            const QString n = QString::fromLatin1(b.name);
            if (n == "LF") return "2200";
            if (n == "MF") return "630";
            return n.endsWith("cm") ? n : n.chopped(1);
        }
    return QString();
}

// ---- comparison columns -------------------------------------------------------------------

QString wattsText(int dBm) { return dBmToWatts(dBm); }

int wattsToDbm(int watts)
{
    if (watts <= 0)
        return 50;   // C# fell back to 50 when the log failed
    return int(10.0 * std::log10(double(watts) * 1000.0));
}

QString cwLabel(int snr, int dBm, int compareDbm)
{
    const int cwL = snr + (compareDbm - dBm);
    if (cwL < -10) return "unusable";
    if (cwL < -6)  return "very weak";
    if (cwL < -2)  return "copyable";
    if (cwL < 3)   return "readable";
    if (cwL < 8)   return "solid";
    if (cwL < 13)  return "very good";
    return "strong";
}

QString ssbLabel(int snr, int dBm, int compareDbm)
{
    const int ssbL = snr + (compareDbm - dBm);
    if (ssbL < 5)  return "unusable";
    if (ssbL < 10) return "borderline";
    if (ssbL < 12) return "workable";
    if (ssbL < 15) return "good";
    return "excellent";
}

int kmToMiles(int km) { return int(std::nearbyint(km * 0.621371)); }   // default rounding = half to even
int milesToKm(int miles) { return int(miles * 1.609); }
QString snrText(int snr) { return snr > 0 ? "+" + QString::number(snr) : QString::number(snr); }

// ---- cycle timing -------------------------------------------------------------------------

QDateTime cycleStart(const QDateTime &utcNow)
{
    const QDateTime t = utcNow.toUTC().addSecs(2);   // odd:59 belongs to the next even minute
    const QTime tm = t.time();
    QDateTime s(t.date(), QTime(tm.hour(), tm.minute() - tm.minute() % 2), QTimeZone::utc());
    return s;
}

int recordDurationMs(const QDateTime &utcNow)
{
    const QDateTime end = cycleStart(utcNow).addSecs(110);
    const qint64 ms = utcNow.toUTC().msecsTo(end);
    return int(qMax<qint64>(ms, 60000));
}

// ---- wsprd --------------------------------------------------------------------------------

QString wsprdExecutable(const QString &dir)
{
#ifdef Q_OS_WIN
    const QString name = "wsprd.exe";
#else
    const QString name = "wsprd";
#endif
    // 1. the configured folder, 2. "wsprd" next to the program, 3. next to the program, 4. PATH
    // a path to the program itself is accepted too ("C:\\WSJT\\wsjtx\\bin\\wsprd.exe")
    const QFileInfo given(dir.trimmed());
    if (!dir.trimmed().isEmpty() && given.isFile())
        return QDir::toNativeSeparators(given.absoluteFilePath());
    QStringList dirs;
    if (!dir.trimmed().isEmpty())
        dirs << dir.trimmed();
    if (QCoreApplication::instance()) {
        const QString app = QCoreApplication::applicationDirPath();
        dirs << QDir(app).filePath("wsprd") << app;
    }
    for (const QString &d : std::as_const(dirs)) {
        const QString p = QDir(d).filePath(name);
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    return QStandardPaths::findExecutable("wsprd");
}

QStringList wsprdArguments(const QString &dataDir, const QString &dialMHz, bool deep, bool quick,
                           int osd, const QString &wavPath)
{
    QStringList a{"-a", QDir::toNativeSeparators(dataDir), "-f", dialMHz};
    if (deep)
        a << "-d";
    else if (quick)
        a << "-q";
    if (osd > 0)
        a << "-o" << QString::number(qMin(osd, 5));
    a << QDir::toNativeSeparators(wavPath);
    return a;
}

// ---- wsprnet.org --------------------------------------------------------------------------

QList<QPair<QString, QString>> wsprnetForm(const RxSpot &s, const QString &version, const QString &rcall,
                                           const QString &rgrid, const QString &rqrg)
{
    return {
        {"function", "wspr"},
        {"date", s.datetime.toUTC().toString("yyMMdd")},
        {"time", s.datetime.toUTC().toString("HHmm")},
        {"sig", QString::number(s.snr)},
        {"dt", QString::number(s.dt, 'f', 1)},
        {"drift", QString::number(s.drift)},
        {"tqrg", QString::number(s.frequency, 'f', 6)},
        {"tcall", s.txSign},
        {"tgrid", s.txLoc},
        {"dbm", QString::number(s.power)},
        {"version", version},
        {"rcall", rcall},
        {"rgrid", rgrid},
        {"rqrg", rqrg},
        {"mode", "2"},
    };
}

bool isOwnCall(const QString &txSign, const QString &ownCall)
{
    auto bare = [](QString c) { c = c.trimmed().toUpper(); c.remove('<'); c.remove('>'); return c; };
    const QString sign = bare(txSign), own = bare(ownCall);
    if (sign.isEmpty() || own.isEmpty())
        return false;
    if (sign == own)
        return true;
    // base call: the longest part of a compound call ("G4GCI" in "G4GCI/P" or "F/G4GCI")
    auto base = [](const QString &c) {
        QString best;
        for (const QString &p : c.split('/'))
            if (p.size() > best.size())
                best = p;
        return best;
    };
    return base(sign) == base(own);
}

bool shouldUpload(const RxSpot &s, const QString &ownCall, bool allowOwnCall)
{
    if (s.isNil() || s.txSign.trimmed().isEmpty())
        return false;
    if (!allowOwnCall && isOwnCall(s.txSign, ownCall))
        return false;
    return true;
}

// ---- debug log ----------------------------------------------------------------------------

namespace {
QMutex g_logMutex;
QString g_logPath;
} // namespace

QString defaultDebugLogPath()
{
#ifdef Q_OS_WIN
    return "C:/Users/Public/wspr_debug.txt";
#else
    return QDir::home().filePath("wspr_debug.txt");
#endif
}

void setDebugLogPath(const QString &path)
{
    QMutexLocker l(&g_logMutex);
    g_logPath = path;
}

static QString logPathLocked() { return g_logPath.isEmpty() ? defaultDebugLogPath() : g_logPath; }

void debugLog(const QString &text)
{
    QMutexLocker l(&g_logMutex);
    QFile f(logPathLocked());
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream(&f) << QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss") << "Z " << text << '\n';
}

void clearDebugLog()
{
    QMutexLocker l(&g_logMutex);
    QFile f(logPathLocked());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    QTextStream(&f) << QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss") << "Z ===== log cleared =====\n";
}

QString defaultDataDir()
{
#ifdef Q_OS_WIN
    return "C:/WSPR_Sked";
#else
    return QDir::home().filePath("WSPR_Sked");
#endif
}

} // namespace Rx
