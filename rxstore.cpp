#include "rxstore.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimeZone>
#include <QUuid>

namespace {

bool isConnectionError(const QSqlError &e)
{
    if (e.type() == QSqlError::ConnectionError)
        return true;
    const QString code = e.nativeErrorCode();
    return code == "2006" || code == "2013";
}

QString sqlTime(const QDateTime &t)
{
    return t.toUTC().toString("yyyy-MM-dd HH:mm:ss");
}

QDateTime utcFromDb(const QVariant &v)
{
    const QDateTime d = v.toDateTime();
    return QDateTime(d.date(), d.time(), QTimeZone::utc());   // stored as UTC, read back without conversion
}

RxSpot spotFromQuery(const QSqlQuery &q)
{
    RxSpot s;
    s.datetime = utcFromDb(q.value("datetime"));
    s.band = q.value("band").toInt();
    s.txSign = q.value("tx_sign").toString();
    s.txLoc = q.value("tx_loc").toString();
    s.frequency = q.value("frequency").toDouble();
    s.power = q.value("power").toInt();
    s.snr = q.value("snr").toInt();
    s.drift = q.value("drift").toInt();
    s.distance = q.value("distance").toInt();
    s.azimuth = q.value("azimuth").toInt();
    s.reporter = q.value("reporter").toString();
    s.reporterLoc = q.value("reporter_loc").toString();
    s.dt = q.value("dt").toDouble();
    return s;
}

} // namespace

RxStore::RxStore(const DbSettings &s, const QString &database)
    : m_settings(s), m_database(database),
      m_connPrefix("rx" + QUuid::createUuid().toString(QUuid::Id128).left(8))
{
}

RxStore::~RxStore()
{
    for (const QString &c : std::as_const(m_connections)) {
        { QSqlDatabase d = QSqlDatabase::database(c, false); d.close(); }
        QSqlDatabase::removeDatabase(c);
    }
}

void RxStore::setSettings(const DbSettings &s) { m_settings = s; }

QSqlDatabase RxStore::db(const QString &database)
{
    const QString name = m_connPrefix + '_' + database;
    QSqlDatabase d;
    if (QSqlDatabase::contains(name)) {
        d = QSqlDatabase::database(name, false);
    } else {
        d = QSqlDatabase::addDatabase("QMYSQL", name);
        m_connections << name;
    }
    if (d.hostName() != m_settings.host || d.userName() != m_settings.user || d.password() != m_settings.password
        || d.port() != m_settings.port) {
        d.close();
        d.setHostName(m_settings.host);
        d.setPort(m_settings.port);
        d.setUserName(m_settings.user);
        d.setPassword(m_settings.password);
        d.setDatabaseName(database);
        d.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3");
    }
    if (!d.isOpen() && !d.open())
        m_lastError = d.lastError().text();
    return d;
}

// Runs a prepared statement, reconnecting once if the server dropped the connection.
// Returns the executed query, or nullopt.
static std::optional<QSqlQuery> run(QSqlDatabase d, const QString &sql, const QVariantList &binds, QString *err)
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!d.isOpen() && !d.open()) {
            *err = d.lastError().text();
            return std::nullopt;
        }
        QSqlQuery q(d);
        q.setForwardOnly(true);
        q.prepare(sql);
        for (const QVariant &b : binds)   // an empty QString would go to MySQL as NULL; NOT NULL text columns need ''
            q.addBindValue(b.metaType().id() == QMetaType::QString && b.toString().isNull() ? QVariant(QStringLiteral("")) : b);
        if (q.exec())
            return q;
        *err = q.lastError().text();
        if (attempt == 0 && isConnectionError(q.lastError())) {
            d.close();
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

bool RxStore::insert(const RxSpot &s)
{
    if (s.txSign.isEmpty())
        return false;
    const QString sql =
        "INSERT IGNORE INTO received(datetime,band,tx_sign,tx_loc,frequency,power,snr,drift,distance,azimuth,"
        "reporter,reporter_loc,dt) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)";
    return run(db(m_database), sql,
               {sqlTime(s.datetime), s.band, s.txSign, s.txLoc, s.frequency, s.power, s.snr, s.drift,
                s.distance, s.azimuth, s.reporter, s.reporterLoc, s.dt},
               &m_lastError).has_value();
}

std::optional<int> RxStore::count()
{
    auto q = run(db(m_database), "SELECT COUNT(*) FROM received", {}, &m_lastError);
    if (!q || !q->next())
        return std::nullopt;
    return q->value(0).toInt();
}

std::optional<QVector<RxSpot>> RxStore::select(const RxQuery &rq)
{
    auto q = run(db(m_database), rq.sql, rq.binds, &m_lastError);
    if (!q)
        return std::nullopt;
    QVector<RxSpot> v;
    while (q->next())
        v.append(spotFromQuery(*q));
    return v;
}

std::optional<QVector<RxSpot>> RxStore::latest(int maxRows)
{
    return select({"SELECT * FROM received ORDER BY datetime DESC, frequency ASC LIMIT " + QString::number(maxRows), {}});
}

RxQuery RxStore::filterQuery(const RxFilter &f, int maxRows)
{
    QStringList where;
    QVariantList binds;
    if (f.useDates) {
        where << "datetime >= ? AND datetime <= ?";
        binds << sqlTime(f.from) << sqlTime(f.to);
    }
    const auto &bands = Rx::bandRanges();
    if (f.bandIndex > 0 && f.bandIndex < bands.size()) {
        where << "frequency >= ? AND frequency < ?";
        binds << bands[f.bandIndex].lo << bands[f.bandIndex].hi;
    }
    QString call = f.call.trimmed().toUpper();
    call.remove('*');
    if (!call.isEmpty()) {
        call.replace('\\', "\\\\").replace('%', "\\%").replace('_', "\\_");
        where << "(tx_sign LIKE ? OR tx_sign LIKE ?)";
        binds << call + '%' << '<' + call + '%';
    }
    if (f.minKm) {
        where << "distance >= ?";
        binds << *f.minKm;
    }
    if (f.maxKm) {
        where << "distance <= ?";
        binds << *f.maxKm;
    }
    QString sql = "SELECT * FROM received";
    if (!where.isEmpty())
        sql += " WHERE " + where.join(" AND ");
    sql += " ORDER BY datetime DESC, frequency ASC LIMIT " + QString::number(maxRows);
    return {sql, binds};
}

std::optional<QVector<RxSpot>> RxStore::filtered(const RxFilter &f, int maxRows)
{
    return select(filterQuery(f, maxRows));
}

bool RxStore::remove(const QDateTime &datetime, const QString &txSign, double frequency)
{
    auto q = run(db(m_database),
                 "DELETE FROM received WHERE datetime = ? AND tx_sign = ? AND ABS(frequency - ?) < 0.0000005",
                 {sqlTime(datetime), txSign, frequency}, &m_lastError);
    return q.has_value();
}

std::optional<RxConfig> RxStore::readConfig()
{
    auto q = run(db(m_database), "SELECT * FROM rxconfig", {}, &m_lastError);
    if (!q)
        return std::nullopt;
    std::optional<RxConfig> c;
    while (q->next()) {   // last row wins, as C#
        RxConfig r;
        r.deep = q->value("deep").toBool();
        r.quick = q->value("quick").toBool();
        r.osd = qBound(0, q->value("osd").toInt(), 5);
        r.upload = q->value("upload").toBool();
        c = r;
    }
    return c;
}

bool RxStore::saveConfig(const RxConfig &c)
{
    return run(db(m_database),
               "INSERT INTO rxconfig(id,deep,quick,osd,upload) VALUES(0,?,?,?,?) "
               "ON DUPLICATE KEY UPDATE deep = VALUES(deep), quick = VALUES(quick), osd = VALUES(osd), "
               "upload = VALUES(upload)",
               {c.deep, c.quick, c.osd, c.upload}, &m_lastError).has_value();
}

std::optional<RxSettings> RxStore::readRxSettings()
{
    auto q = run(db("wspr_configs"), "SELECT * FROM rxsettings", {}, &m_lastError);
    if (!q)
        return std::nullopt;
    std::optional<RxSettings> r;
    while (q->next()) {
        const QSqlRecord rec = q->record();
        auto has = [&](const char *c) { return rec.indexOf(c) >= 0 && !q->value(c).isNull(); };
        RxSettings s;
        if (has("outputname")) s.outputName = q->value("outputname").toString();
        if (has("outputdevice")) s.outputDevice = q->value("outputdevice").toInt();
        if (has("inputname")) s.inputName = q->value("inputname").toString();
        if (has("inputdevice")) s.inputDevice = q->value("inputdevice").toInt();
        if (has("outlevel")) s.outLevel = q->value("outlevel").toInt();
        if (has("inlevel")) s.inLevel = q->value("inlevel").toInt();
        if (has("samedev")) s.sameDevice = q->value("samedev").toBool();
        if (has("wsprdpath")) s.wsprdPath = q->value("wsprdpath").toString();
        r = s;
    }
    return r;
}

bool RxStore::saveRxSettings(const RxSettings &s)
{
    QString path = s.wsprdPath;
    path.replace('\\', '/');   // as C#
    return run(db("wspr_configs"),
               "INSERT INTO rxsettings(id,outputname,outputdevice,inputname,inputdevice,outlevel,inlevel,wsprdpath,samedev) "
               "VALUES(0,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE outputname = VALUES(outputname), "
               "outputdevice = VALUES(outputdevice), inputname = VALUES(inputname), inputdevice = VALUES(inputdevice), "
               "outlevel = VALUES(outlevel), inlevel = VALUES(inlevel), wsprdpath = VALUES(wsprdpath), "
               "samedev = VALUES(samedev)",
               {s.outputName, s.outputDevice, s.inputName, s.inputDevice, s.outLevel, s.inLevel, path, s.sameDevice},
               &m_lastError).has_value();
}
