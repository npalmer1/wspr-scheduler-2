#include "slotstore.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlRecord>
#include <functional>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>
#include <cmath>

namespace {

const char *kSlotColumns =
    "`date`,`time`,`frequency`,`offset`,`power`,`powerw`,`antenna`,`tuner`,`switch`,`switchport`,"
    "`end`,`active`,`repeating`,`timeend`,`rpttime`,`parent`,`slotno`,`msgtype`,`rpttype`,`greyoffset`";

QTime parseLooseTime(const QString &s)
{
    // sunrise/sunset are stored as text by the C# app; accept "HH:mm", "HH:mm:ss", "dd/MM/yyyy HH:mm:ss"...
    static const QRegularExpression re(QStringLiteral("(\\d{1,2}):(\\d{2})(?::(\\d{2}))?"));
    const auto m = re.match(s);
    if (!m.hasMatch())
        return {};
    return QTime(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
}

bool isConnectionError(const QSqlError &e)
{
    if (e.type() == QSqlError::ConnectionError)
        return true;
    // MySQL "server has gone away" / "lost connection"
    const QString code = e.nativeErrorCode();
    return code == "2006" || code == "2013";
}

} // namespace

SlotStore::SlotStore(const DbSettings &s)
    : m_settings(s), m_connPrefix(QUuid::createUuid().toString(QUuid::Id128).left(8))
{
}

SlotStore::~SlotStore()
{
    for (const QString &c : std::as_const(m_connections)) {
        { QSqlDatabase d = QSqlDatabase::database(c, false); d.close(); }
        QSqlDatabase::removeDatabase(c);
    }
}

void SlotStore::setSettings(const DbSettings &s)
{
    m_settings = s;
    for (const QString &c : std::as_const(m_connections)) {
        QSqlDatabase d = QSqlDatabase::database(c, false);
        d.close();
    }
    m_hasSwitch2.reset();
}

void SlotStore::setSlotDbName(const QString &name)
{
    m_slotDb = name;
    m_hasSwitch2.reset();
}

QSqlDatabase SlotStore::db(const QString &database)
{
    const QString name = m_connPrefix + '_' + (database.isEmpty() ? QStringLiteral("server") : database);
    QSqlDatabase d;
    if (QSqlDatabase::contains(name)) {
        d = QSqlDatabase::database(name, false);
    } else {
        d = QSqlDatabase::addDatabase("QMYSQL", name);
        m_connections << name;
    }
    if (d.hostName() != m_settings.host || d.userName() != m_settings.user || d.password() != m_settings.password) {
        d.close();
        d.setHostName(m_settings.host);
        d.setPort(m_settings.port);
        d.setUserName(m_settings.user);
        d.setPassword(m_settings.password);
        d.setDatabaseName(database);
        d.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3");
    }
    if (!d.isOpen() && !d.open())
        setError(d.lastError().text());
    return d;
}

// Runs a statement, reconnecting once if the server went away (e.g. after a power cut).
bool SlotStore::exec(QSqlDatabase d, const QString &sql, const QVariantList &binds)
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!d.isOpen() && !d.open()) {
            setError(d.lastError().text());
            return false;
        }
        QSqlQuery q(d);
        q.prepare(sql);
        for (const QVariant &b : binds)   // an empty QString would go to MySQL as NULL; NOT NULL text columns need ''
            q.addBindValue(b.metaType().id() == QMetaType::QString && b.toString().isNull() ? QVariant(QStringLiteral("")) : b);
        if (q.exec())
            return true;
        setError(q.lastError().text());
        if (attempt == 0 && isConnectionError(q.lastError())) {
            d.close();
            continue;
        }
        return false;
    }
    return false;
}

bool SlotStore::slotDbReachable()
{
    QSqlDatabase d = db(m_slotDb);
    if (!d.isOpen())
        return false;
    QSqlQuery q(d);
    if (q.exec("SELECT 1"))
        return true;
    d.close();
    return d.open();
}

bool SlotStore::databaseExists(const QString &name)
{
    QSqlDatabase d = db(QString());
    if (!d.isOpen())
        return false;
    QSqlQuery q(d);
    q.prepare("SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name = ?");
    q.addBindValue(name);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool SlotStore::columnExists(QSqlDatabase &d, const QString &table, const QString &column)
{
    QSqlQuery q(d);
    q.prepare("SELECT COUNT(*) FROM information_schema.columns "
              "WHERE table_schema = DATABASE() AND table_name = ? AND column_name = ?");
    q.addBindValue(table);
    q.addBindValue(column);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

bool SlotStore::addColumnIfMissing(QSqlDatabase &d, const QString &table, const QString &column, const QString &type)
{
    if (columnExists(d, table, column))
        return true;
    return exec(d, QString("ALTER TABLE `%1` ADD COLUMN `%2` %3").arg(table, column, type));
}

// C#: createSlotDatabase - same schema, so the C# app and this one can share it
bool SlotStore::createSlotDatabase(const QString &name)
{
    static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (!safe.match(name).hasMatch()) {
        setError("Invalid database name");
        return false;
    }
    if (!exec(db(QString()), QString("CREATE DATABASE IF NOT EXISTS `%1`").arg(name)))
        return false;
    QSqlDatabase d = db(name);
    if (!exec(d, "CREATE TABLE IF NOT EXISTS `slots` (`date` DATE NOT NULL, `time` TIME NOT NULL, PRIMARY KEY (`date`, `time`))"))
        return false;
    const QList<QPair<QString, QString>> cols = {
        {"frequency", "DOUBLE NOT NULL"}, {"offset", "INT NOT NULL"}, {"power", "INT NOT NULL"},
        {"powerw", "DOUBLE NULL"}, {"antenna", "TEXT NOT NULL"}, {"tuner", "INT(3) NOT NULL"},
        {"switch", "INT(3) NOT NULL"}, {"switchport", "INT NOT NULL"}, {"rotator", "VARCHAR(3) NOT NULL"},
        {"azimuth", "INT NOT NULL"}, {"elevation", "INT NOT NULL"}, {"end", "DATE NULL"},
        {"active", "TINYINT(1) NOT NULL"}, {"repeating", "TINYINT(1) NOT NULL"}, {"timeend", "TIME NOT NULL"},
        {"rpttime", "TINYINT(1) NOT NULL"}, {"parent", "TEXT NOT NULL"}, {"audio", "INT NOT NULL"},
        {"slotno", "INT NOT NULL"}, {"msgtype", "INT NOT NULL"}, {"rpttype", "TINYINT(4) NOT NULL"},
        {"greyoffset", "INT(4) NOT NULL"}, {"switch2", "INT NOT NULL DEFAULT 0"},
        {"switchport2", "INT NOT NULL DEFAULT 0"},
    };
    bool ok = true;
    for (const auto &c : cols)
        ok = addColumnIfMissing(d, "slots", c.first, c.second) && ok;
    m_hasSwitch2.reset();
    return ok;
}

// C#: EnsureIndexes (parent index makes "delete all repeating slots" fast)
void SlotStore::ensureIndexes()
{
    QSqlDatabase d = db(m_slotDb);
    for (const char *sql : {"CREATE INDEX IF NOT EXISTS idx_slots_date ON slots (`date`)",
                            "CREATE INDEX IF NOT EXISTS idx_slots_parent ON slots (`parent`(32))"})
        exec(d, sql);
}

bool SlotStore::hasSwitch2Columns()
{
    if (m_hasSwitch2)
        return *m_hasSwitch2;
    QSqlDatabase s = db(m_slotDb);
    QSqlDatabase w = db("wspr");
    if (!s.isOpen() || !w.isOpen())
        return false;   // don't cache a failure
    m_hasSwitch2 = columnExists(s, "slots", "switch2") && columnExists(s, "slots", "switchport2")
                   && columnExists(w, "antennas", "switch2") && columnExists(w, "antennas", "switchport2");
    return *m_hasSwitch2;
}

static SlotRecord readSlot(const QSqlQuery &q, bool sw2)
{
    SlotRecord r;
    r.date = q.value("date").toDate();
    r.time = q.value("time").toTime();
    r.freq = std::round(q.value("frequency").toDouble() * 10000.0) / 10000.0;
    r.offset = q.value("offset").toInt();
    r.powerdBm = q.value("power").toInt();
    r.powerW = q.value("powerw").toDouble();
    r.antenna = q.value("antenna").toString();
    r.tuner = q.value("tuner").toInt();
    r.sw = q.value("switch").toInt();
    r.swPort = q.value("switchport").toInt();
    r.endDate = q.value("end").toDate();
    if (!r.endDate.isValid())
        r.endDate = QDate(2024, 1, 1);   // C# fallback
    r.active = q.value("active").toBool();
    r.repeating = q.value("repeating").toBool();
    r.endTime = q.value("timeend").toTime();
    r.rptTime = q.value("rpttime").toBool();
    r.parent = q.value("parent").toString();
    r.slotNo = q.value("slotno").toInt();
    r.msgType = q.value("msgtype").toInt();
    r.rptType = q.value("rpttype").toInt();
    r.greyOffset = q.value("greyoffset").toInt();
    if (r.rptType == RptUntilTime || r.rptType == RptAllDay)
        r.greyOffset = 0;
    if (r.greyOffset == 127)
        r.greyOffset = 120;   // older versions: max offset 120 min
    if (sw2) {
        r.sw2 = q.value("switch2").toInt();
        r.swPort2 = q.value("switchport2").toInt();
    }
    return r;
}

std::optional<QMap<QTime, SlotRecord>> SlotStore::loadHour(const QDate &date, int hour)
{
    QMap<QTime, SlotRecord> out;
    const bool sw2 = hasSwitch2Columns();
    const QString sql = QString("SELECT %1%2 FROM slots WHERE `date` = ? AND `time` >= ? AND `time` < ?")
                            .arg(kSlotColumns, sw2 ? ",`switch2`,`switchport2`" : "");
    for (int attempt = 0; attempt < 2; ++attempt) {
        QSqlDatabase d = db(m_slotDb);
        QSqlQuery q(d);
        q.prepare(sql);
        q.addBindValue(date.toString("yyyy-MM-dd"));
        q.addBindValue(QTime(hour, 0).toString("HH:mm:ss"));
        q.addBindValue(hour == 23 ? QString("24:00:00") : QTime(hour + 1, 0).toString("HH:mm:ss"));
        if (q.exec()) {
            while (q.next()) {
                SlotRecord r = readSlot(q, sw2);
                out.insert(r.time, r);
            }
            m_lastError.clear();
            return out;
        }
        setError(q.lastError().text());
        if (!isConnectionError(q.lastError()))
            break;
        d.close();
    }
    return std::nullopt;
}

std::optional<SlotRecord> SlotStore::find(const QDate &date, const QTime &time)
{
    const bool sw2 = hasSwitch2Columns();
    QSqlDatabase d = db(m_slotDb);
    QSqlQuery q(d);
    q.prepare(QString("SELECT %1%2 FROM slots WHERE `date` = ? AND `time` = ?")
                  .arg(kSlotColumns, sw2 ? ",`switch2`,`switchport2`" : ""));
    q.addBindValue(date.toString("yyyy-MM-dd"));
    q.addBindValue(time.toString("HH:mm:ss"));
    if (!q.exec()) {
        setError(q.lastError().text());
        return std::nullopt;
    }
    if (q.next())
        return readSlot(q, sw2);
    return std::nullopt;
}

bool SlotStore::upsert(const SlotRecord &r)
{
    return upsertMany({r});
}

// C#: SaveSlotData. All values are bound (the C# version built the UPDATE part by string
// concatenation, which broke on an antenna name containing an apostrophe).
bool SlotStore::upsertMany(const QVector<SlotRecord> &rows)
{
    if (rows.isEmpty())
        return true;
    const bool sw2 = hasSwitch2Columns();
    QStringList cols = {"date", "time", "frequency", "offset", "power", "powerw", "antenna", "tuner",
                        "rotator", "azimuth", "elevation", "switch", "switchport", "end", "active",
                        "repeating", "timeend", "rpttime", "parent", "audio", "slotno", "msgtype",
                        "rpttype", "greyoffset"};
    if (sw2)
        cols << "switch2" << "switchport2";
    QStringList quoted, marks, updates;
    for (const QString &c : std::as_const(cols)) {
        quoted << '`' + c + '`';
        marks << "?";
        if (c != "date" && c != "time")
            updates << QString("`%1` = VALUES(`%1`)").arg(c);
    }
    const QString sql = QString("INSERT INTO slots (%1) VALUES (%2) ON DUPLICATE KEY UPDATE %3")
                            .arg(quoted.join(','), marks.join(','), updates.join(','));

    for (int attempt = 0; attempt < 2; ++attempt) {
        QSqlDatabase d = db(m_slotDb);
        if (!d.isOpen())
            return false;
        d.transaction();
        QSqlQuery q(d);
        q.prepare(sql);
        bool ok = true;
        for (const SlotRecord &r : rows) {
            q.addBindValue(r.date.toString("yyyy-MM-dd"));
            q.addBindValue(r.time.toString("HH:mm"));
            q.addBindValue(r.freq);
            q.addBindValue(r.offset);
            q.addBindValue(r.powerdBm);
            q.addBindValue(r.powerW);
            q.addBindValue(r.antenna);
            q.addBindValue(r.tuner);
            q.addBindValue(QStringLiteral("0"));   // rotator
            q.addBindValue(0);                     // azimuth
            q.addBindValue(0);                     // elevation
            q.addBindValue(r.sw);
            q.addBindValue(r.swPort);
            q.addBindValue(r.endDate.toString("yyyy-MM-dd"));
            q.addBindValue(r.active ? 1 : 0);
            q.addBindValue(r.repeating ? 1 : 0);
            q.addBindValue(r.endTime.toString("HH:mm"));
            q.addBindValue(r.rptTime ? 1 : 0);
            q.addBindValue(r.parent);
            q.addBindValue(0);                     // audio
            q.addBindValue(r.slotNo);
            q.addBindValue(r.msgType);
            q.addBindValue(r.rptType);
            q.addBindValue(r.greyOffset);
            if (sw2) {
                q.addBindValue(r.sw2);
                q.addBindValue(r.swPort2);
            }
            if (!q.exec()) {
                ok = false;
                break;
            }
        }
        if (ok) {
            if (d.commit())
                return true;
            setError(d.lastError().text());
            return false;
        }
        const QSqlError err = q.lastError();
        setError(err.text());
        d.rollback();
        if (attempt == 0 && isConnectionError(err)) {
            d.close();
            continue;
        }
        return false;
    }
    return false;
}

bool SlotStore::deleteOne(const QDate &date, const QTime &time)
{
    return exec(db(m_slotDb), "DELETE FROM slots WHERE `date` = ? AND `time` = ?",
                {date.toString("yyyy-MM-dd"), time.toString("HH:mm:ss")});
}

bool SlotStore::deleteByParent(const QString &parent)
{
    if (parent.trimmed().isEmpty()) {   // C# would have deleted every parent-less slot here
        setError("No parent");
        return false;
    }
    return exec(db(m_slotDb), "DELETE FROM slots WHERE `parent` = ?", {parent});
}

bool SlotStore::deleteBetween(const QDate &from, const QDate &to)
{
    return exec(db(m_slotDb), "DELETE FROM slots WHERE `date` >= ? AND `date` <= ?",
                {from.toString("yyyy-MM-dd"), to.toString("yyyy-MM-dd")});
}

bool SlotStore::deleteBefore(const QDate &cutoff)
{
    return exec(db(m_slotDb), "DELETE FROM slots WHERE `date` < ?", {cutoff.toString("yyyy-MM-dd")});
}

QVector<AntennaInfo> SlotStore::loadAntennas()
{
    QVector<AntennaInfo> out;
    const bool sw2 = hasSwitch2Columns();
    QSqlDatabase d = db("wspr");
    QSqlQuery q(d);
    if (!q.exec(QString("SELECT `antno`,`antenna`,`description`,`switch`,`tuner`,`switchport`%1 FROM antennas")
                    .arg(sw2 ? ",`switch2`,`switchport2`" : ""))) {
        setError(q.lastError().text());
        return out;
    }
    while (q.next()) {
        AntennaInfo a;
        a.antNo = q.value(0).toInt();
        a.name = q.value(1).toString();
        a.description = q.value(2).toString();
        a.sw = q.value(3).toInt();
        a.tuner = q.value(4).toInt();
        a.swPort = q.value(5).toInt();
        if (sw2) {
            a.sw2 = q.value(6).toInt();
            a.swPort2 = q.value(7).toInt();
        }
        out << a;
    }
    return out;
}

bool SlotStore::saveAntenna(const AntennaInfo &a)
{
    const bool sw2 = hasSwitch2Columns();
    QSqlDatabase d = db("wspr");
    // C# tried INSERT, then an UPDATE with the values pasted into the SQL; one bound statement here
    QString cols = "AntNo,Antenna,Description,Switch,Tuner,Rotator,SwitchPort,TunerPort,Azimuth";
    QString vals = "?,?,?,?,?,0,?,0,0";
    QString upd = "Antenna = VALUES(Antenna), Description = VALUES(Description), Switch = VALUES(Switch), "
                  "Tuner = VALUES(Tuner), Rotator = 0, SwitchPort = VALUES(SwitchPort), TunerPort = 0, Azimuth = 0";
    QVariantList b{a.antNo, a.name, a.description, a.sw, a.tuner, a.swPort};
    if (sw2) {
        cols += ",Switch2,SwitchPort2";
        vals += ",?,?";
        upd += ", Switch2 = VALUES(Switch2), SwitchPort2 = VALUES(SwitchPort2)";
        b << a.sw2 << a.swPort2;
    }
    return exec(d, QString("INSERT INTO antennas(%1) VALUES(%2) ON DUPLICATE KEY UPDATE %3").arg(cols, vals, upd), b);
}

bool SlotStore::deleteAntenna(int antNo)
{
    return exec(db("wspr"), "DELETE FROM antennas WHERE AntNo = ?", {antNo});
}

bool SlotStore::renameAntennaInSlots(const QString &oldName, const QString &newName, const QDate &fromDate)
{
    if (oldName == newName)
        return true;
    return exec(db(m_slotDb), "UPDATE slots SET `Antenna` = ? WHERE `Antenna` = ? AND `Date` >= ?",
                {newName, oldName, fromDate.toString("yyyy-MM-dd")});
}

static bool hardwareTable(const QString &t)
{
    return t == "switches" || t == "tuners" || t == "filters";
}

std::optional<QVector<HardwareInfo>> SlotStore::loadHardware(const QString &table)
{
    if (!hardwareTable(table)) {
        setError("Unknown table " + table);
        return std::nullopt;
    }
    QSqlQuery q(db("wspr"));
    if (!q.exec(QString("SELECT * FROM `%1` ORDER BY Id").arg(table))) {
        setError(q.lastError().text());
        return std::nullopt;
    }
    QVector<HardwareInfo> out;
    while (q.next()) {
        const QSqlRecord rec = q.record();
        auto str = [&](const char *c) { return rec.indexOf(c) >= 0 ? q.value(c).toString() : QString(); };
        HardwareInfo h;
        h.id = q.value("Id").toInt();
        h.name = str("Name");
        h.protocol = str("Protocol");
        h.port = str("Port");
        h.ip = str("IP");
        h.baud = str("Baud");
        h.serial = str("Serial");
        h.type = str("Type");
        h.channels = rec.indexOf("Channels") >= 0 ? q.value("Channels").toInt() : 1;
        out << h;
    }
    return out;
}

bool SlotStore::saveHardware(const QString &table, const HardwareInfo &h)
{
    if (!hardwareTable(table)) {
        setError("Unknown table " + table);
        return false;
    }
    // C#: INSERT, then an UPDATE with the values pasted into the SQL
    return exec(db("wspr"),
                QString("INSERT INTO `%1`(Id,Name,Protocol,Port,IP,Baud,Serial,Type,Channels,ChannelType) "
                        "VALUES(?,?,?,?,?,?,?,?,?,'0') ON DUPLICATE KEY UPDATE Name = VALUES(Name), "
                        "Protocol = VALUES(Protocol), Port = VALUES(Port), IP = VALUES(IP), Baud = VALUES(Baud), "
                        "Serial = VALUES(Serial), Type = VALUES(Type), Channels = VALUES(Channels), ChannelType = '0'")
                    .arg(table),
                {h.id, h.name, h.protocol, h.port, h.ip, h.baud, h.serial, h.type, h.channels});
}

bool SlotStore::saveSelectedFilter(int configId, int filter)
{
    QSqlDatabase d = db("wspr_configs");
    if (!addColumnIfMissing(d, "settings", "selectedFilter", "INT NOT NULL"))
        return false;
    if (configId > 0)
        return exec(d, "UPDATE settings SET selectedFilter = ? WHERE ConfigID = ?", {filter, configId});
    return exec(d, "UPDATE settings SET selectedFilter = ?", {filter});
}

QVector<FrequencyLevel> SlotStore::loadFrequencies()
{
    QVector<FrequencyLevel> out;
    QSqlQuery q(db("wspr"));
    if (!q.exec("SELECT Frequency, AudioLevel FROM frequencies ORDER BY Frequency")) {
        setError(q.lastError().text());
        return out;
    }
    while (q.next())
        out << FrequencyLevel{q.value(0).toDouble(), q.value(1).toDouble()};
    return out;
}

bool SlotStore::saveFrequency(const FrequencyLevel &f)
{
    QSqlDatabase d = db("wspr");
    // C# wrote Slot.Switch2 into RXAudioLevel by mistake; 1.0 (its UPDATE value) is used here
    if (columnExists(d, "frequencies", "RXAudioLevel") && columnExists(d, "frequencies", "Region"))
        return exec(d, "INSERT INTO frequencies(Frequency,AudioLevel,RXAudioLevel,Region) VALUES(?,?,1.0,0) "
                       "ON DUPLICATE KEY UPDATE AudioLevel = VALUES(AudioLevel)",
                    {f.mhz, f.level});
    return exec(d, "INSERT INTO frequencies(Frequency,AudioLevel) VALUES(?,?) "
                   "ON DUPLICATE KEY UPDATE AudioLevel = VALUES(AudioLevel)",
                {f.mhz, f.level});
}

bool SlotStore::deleteFrequency(double mhz)
{
    return exec(db("wspr"), "DELETE FROM frequencies WHERE ABS(Frequency - ?) < 0.0000005", {mhz});
}

std::optional<AppConfig> SlotStore::loadConfig()
{
    QSqlDatabase d = db("wspr_configs");
    QSqlQuery q(d);
    if (!q.exec("SELECT * FROM settings")) {
        setError(q.lastError().text());
        return std::nullopt;
    }
    std::optional<AppConfig> cfg;
    while (q.next()) {   // C# reads every row; the last one wins
        const QSqlRecord rec = q.record();
        auto has = [&](const char *c) { return rec.indexOf(c) >= 0 && !q.value(c).isNull(); };
        AppConfig c;
        if (has("ConfigID")) c.configId = q.value("ConfigID").toInt();
        if (has("Callsign")) c.callsign = q.value("Callsign").toString();
        if (has("BaseCall")) c.baseCall = q.value("BaseCall").toString();
        if (has("DefaultF")) c.defaultF = q.value("DefaultF").toDouble();
        if (has("Offset")) c.defaultOffset = q.value("Offset").toInt();
        if (has("Power")) c.defaultdBm = q.value("Power").toInt();
        if (has("Locator")) c.locator = q.value("Locator").toString();
        if (has("LocatorLong")) c.longLocator = q.value("LocatorLong").toBool();
        if (has("DefaultAnt")) c.defaultAnt = q.value("DefaultAnt").toString();
        if (has("AllowType2")) c.allowType2 = q.value("AllowType2").toBool();
        if (has("oneMsg")) c.oneMsg = q.value("oneMsg").toBool();
        if (has("TimeZone")) c.localTime = q.value("TimeZone").toString() == "LT";
        if (has("stopRX")) c.stopRX = q.value("stopRX").toBool();
        if (has("selectedFilter")) c.selectedFilter = q.value("selectedFilter").toInt();
        if (has("Alpha") && q.value("Alpha").toDouble() > 0.0 && q.value("Alpha").toDouble() <= 1.0)
            c.alpha = q.value("Alpha").toDouble();
        if (has("HamlibPath")) c.hamlibPath = q.value("HamlibPath").toString().trimmed();
        if (has("SlotDB") && !q.value("SlotDB").toString().trimmed().isEmpty())
            c.slotDb = q.value("SlotDB").toString().trimmed();
        cfg = c;
    }
    return cfg;
}

// C# never actually called SaveDBName, so the main/test choice was lost on restart.
bool SlotStore::saveSlotDbName(int configId, const QString &name)
{
    QSqlDatabase d = db("wspr_configs");
    if (!addColumnIfMissing(d, "settings", "SlotDB", "TEXT NOT NULL"))
        return false;
    if (configId > 0)
        return exec(d, "UPDATE settings SET SlotDB = ? WHERE ConfigID = ?", {name, configId});
    return exec(d, "UPDATE settings SET SlotDB = ?", {name});
}

bool SlotStore::saveHamlibPath(int configId, const QString &path)
{
    QString p = path;
    p.replace('\\', '/');   // C#: "make mysql friendly"
    QSqlDatabase d = db("wspr_configs");
    if (configId > 0)
        return exec(d, "UPDATE settings SET HamlibPath = ? WHERE ConfigID = ?", {p, configId});
    return exec(d, "UPDATE settings SET HamlibPath = ?", {p});
}

std::optional<RigctlSettings> SlotStore::loadRigctl()
{
    QSqlDatabase d = db("wspr");
    addColumnIfMissing(d, "rigctl", "VOX", "BOOLEAN NOT NULL");   // C#: addRigctlField
    addColumnIfMissing(d, "rigctl", "ppm", "DOUBLE NOT NULL DEFAULT 0");
    QSqlQuery q(d);
    if (!q.exec("SELECT * FROM rigctl")) {
        setError(q.lastError().text());
        return std::nullopt;
    }
    std::optional<RigctlSettings> r;
    while (q.next()) {   // last row wins, as C#
        const QSqlRecord rec = q.record();
        auto str = [&](const char *c) { return rec.indexOf(c) >= 0 ? q.value(c).toString() : QString(); };
        RigctlSettings s;
        s.radio = str("Radio");
        s.comPort = str("COMport");
        s.baud = str("Baud");
        s.ipv4 = str("IPv4");
        s.port = str("Port");
        s.noRigctld = rec.indexOf("norigctld") >= 0 && q.value("norigctld").toBool();
        s.vox = rec.indexOf("VOX") >= 0 && q.value("VOX").toBool();
        s.ppm = rec.indexOf("ppm") >= 0 ? q.value("ppm").toDouble() : 0.0;
        r = s;
    }
    return r;
}

bool SlotStore::saveRigctl(const RigctlSettings &r)
{
    QSqlDatabase d = db("wspr");
    addColumnIfMissing(d, "rigctl", "VOX", "BOOLEAN NOT NULL");
    addColumnIfMissing(d, "rigctl", "ppm", "DOUBLE NOT NULL DEFAULT 0");
    // C# tried INSERT and fell back to UPDATE ... WHERE RigctlID = 0; same effect in one statement
    return exec(d,
                "INSERT INTO rigctl(RigctlID, Radio, COMport, Baud, IPv4, Port, norigctld, VOX, ppm) "
                "VALUES(0,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE Radio = VALUES(Radio), COMport = VALUES(COMport), "
                "Baud = VALUES(Baud), IPv4 = VALUES(IPv4), Port = VALUES(Port), norigctld = VALUES(norigctld), "
                "VOX = VALUES(VOX), ppm = VALUES(ppm)",
                {r.radio, r.comPort, r.baud, r.ipv4, r.port, r.noRigctld, r.vox, r.ppm});
}

std::optional<SunTimes> SlotStore::sunTimes(const QString &locator4, const QDate &date)
{
    QSqlDatabase d = db("wspr_grey");
    QSqlQuery q(d);
    q.prepare("SELECT sunrise, sunset FROM sunrise_sunset WHERE locator = ? AND date = ?");
    q.addBindValue(locator4);
    q.addBindValue(date.toString("MM-dd"));
    if (!q.exec() || !q.next())
        return std::nullopt;
    SunTimes t{parseLooseTime(q.value(0).toString()), parseLooseTime(q.value(1).toString())};
    if (!t.rise.isValid() || !t.set.isValid())
        return std::nullopt;
    return t;
}

QHash<QString, SunTimes> SlotStore::allSunTimes(const QString &locator4)
{
    QHash<QString, SunTimes> out;
    QSqlDatabase d = db("wspr_grey");
    QSqlQuery q(d);
    q.prepare("SELECT date, sunrise, sunset FROM sunrise_sunset WHERE locator = ?");
    q.addBindValue(locator4);
    if (!q.exec())
        return out;
    while (q.next()) {
        SunTimes t{parseLooseTime(q.value(1).toString()), parseLooseTime(q.value(2).toString())};
        if (t.rise.isValid() && t.set.isValid())
            out.insert(q.value(0).toString(), t);
    }
    return out;
}

bool SlotStore::saveSunTimes(const QString &locator4, const QMap<QString, SunTimes> &days)
{
    {
        QSqlDatabase c = db("wspr_configs");
        exec(c, "CREATE DATABASE IF NOT EXISTS wspr_grey");
    }
    QSqlDatabase d = db("wspr_grey");
    if (!exec(d, "CREATE TABLE IF NOT EXISTS sunrise_sunset (locator VARCHAR(8) NOT NULL, date VARCHAR(5) NOT NULL, "
                 "sunrise TEXT, sunset TEXT, PRIMARY KEY (locator, date))"))
        return false;
    if (!d.transaction()) {
        setError(d.lastError().text());
        return false;
    }
    for (auto it = days.constBegin(); it != days.constEnd(); ++it) {
        if (!exec(d, "INSERT INTO sunrise_sunset(locator, date, sunrise, sunset) VALUES(?,?,?,?) "
                     "ON DUPLICATE KEY UPDATE sunrise = VALUES(sunrise), sunset = VALUES(sunset)",
                  {locator4, it.key(), it->rise.toString("HH:mm"), it->set.toString("HH:mm")})) {
            d.rollback();
            return false;
        }
    }
    if (!d.commit()) {
        setError(d.lastError().text());
        return false;
    }
    return true;
}

bool SlotStore::saveAlpha(int configId, double alpha)
{
    QSqlDatabase d = db("wspr_configs");
    if (configId > 0)
        return exec(d, "UPDATE settings SET Alpha = ? WHERE ConfigID = ?", {alpha, configId});
    return exec(d, "UPDATE settings SET Alpha = ?", {alpha});
}

bool SlotStore::saveSettings(const AppConfig &c, int msgType)
{
    QSqlDatabase d = db("wspr_configs");
    addColumnIfMissing(d, "settings", "PowerW", "DOUBLE NOT NULL DEFAULT 0");   // in C#'s table, not in older ones
    QSqlQuery q(d);
    if (!q.exec("SELECT COUNT(*) FROM settings") || !q.next()) {
        setError(q.lastError().text());
        return false;
    }
    const double watts = dBmToWatts(c.defaultdBm).toDouble();
    addColumnIfMissing(d, "settings", "stopRX", "TINYINT(1) NOT NULL DEFAULT 0");
    const QVariantList v{c.callsign, c.baseCall, c.defaultOffset, c.defaultF, c.defaultdBm, watts, c.locator,
                         c.longLocator, c.defaultAnt, c.alpha, msgType, c.allowType2, c.oneMsg, c.stopRX};
    if (q.value(0).toInt() == 0)
        return exec(d, "INSERT INTO settings(ConfigID, Callsign, BaseCall, `Offset`, DefaultF, Power, PowerW, Locator, "
                       "LocatorLong, DefaultAnt, Alpha, MsgType, AllowType2, oneMsg, stopRX) VALUES(1,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    v);
    QVariantList w = v;
    QString where;
    if (c.configId > 0) {
        where = " WHERE ConfigID = ?";
        w << c.configId;
    }
    return exec(d, "UPDATE settings SET Callsign = ?, BaseCall = ?, `Offset` = ?, DefaultF = ?, Power = ?, PowerW = ?, "
                   "Locator = ?, LocatorLong = ?, DefaultAnt = ?, Alpha = ?, MsgType = ?, AllowType2 = ?, oneMsg = ?, "
                   "stopRX = ?" + where,
                w);
}

bool SlotStore::changeOwnPassword(const QString &pw)
{
    static const QRegularExpression ok("^[A-Za-z0-9.,_ -]{4,64}$");
    if (!ok.match(pw).hasMatch()) {
        setError("password must be 4-64 letters, digits or . , - _ space");
        return false;
    }
    // the character check above means the text can go into the statement as a literal safely
    const QString lit = "'" + pw + "'";
    const QString name = "pwchange_" + QUuid::createUuid().toString(QUuid::Id128).left(8);
    bool done = false;
    {
        QSqlDatabase d = QSqlDatabase::addDatabase("QMYSQL", name);   // no default database needed
        d.setHostName(m_settings.host);
        d.setPort(m_settings.port);
        d.setUserName(m_settings.user);
        d.setPassword(m_settings.password);
        d.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3");
        if (!d.open()) {
            setError(d.lastError().text());
        } else {
            QStringList errors;
            for (const QString &sql : {"ALTER USER CURRENT_USER() IDENTIFIED BY " + lit,
                                       "SET PASSWORD = PASSWORD(" + lit + ")", "SET PASSWORD = " + lit}) {
                QSqlQuery q(d);
                if (q.exec(sql)) {
                    done = true;
                    break;
                }
                errors << q.lastError().databaseText();
            }
            if (!done)
                setError(errors.join(" / "));
            d.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
    if (done) {   // the pooled connections still hold the old password: reopen them next time
        for (const QString &c : std::as_const(m_connections)) {
            QSqlDatabase x = QSqlDatabase::database(c, false);
            x.close();
        }
        m_settings.password = pw;
    }
    return done;
}

bool SlotStore::canLogIn(const DbSettings &s, QString *error)
{
    const QString name = "pwcheck_" + QUuid::createUuid().toString(QUuid::Id128).left(8);
    bool ok = false;
    {
        QSqlDatabase d = QSqlDatabase::addDatabase("QMYSQL", name);
        d.setHostName(s.host);
        d.setPort(s.port);
        d.setUserName(s.user);
        d.setPassword(s.password);
        d.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3");
        ok = d.open();
        if (!ok && error)
            *error = d.lastError().text();
        d.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

// C#: createRigTable - wspr.rigs with the C# columns
static bool ensureRigTable(SlotStore *st, QSqlDatabase &d, const std::function<bool(QSqlDatabase &, const QString &)> &run)
{
    Q_UNUSED(st)
    return run(d, "CREATE TABLE IF NOT EXISTS rigs (id INT NOT NULL PRIMARY KEY, rigname TEXT NOT NULL, type INT NOT NULL DEFAULT 1, "
                  "selected INT NOT NULL DEFAULT 0, TXcommand TEXT NOT NULL, RXcommand TEXT NOT NULL, TXptt TEXT NULL, "
                  "command1 TEXT NULL, command2 TEXT NULL, command3 TEXT NULL, command4 TEXT NULL, reply1 TEXT NULL, "
                  "reply2 TEXT NULL, reply3 TEXT NULL, reply4 TEXT NULL, Protocol TEXT NULL, Port TEXT NULL, IP TEXT NULL, "
                  "Baud TEXT NULL, Serial TEXT NULL, URL TEXT NULL)");
}

std::optional<QVector<OtherRig>> SlotStore::loadOtherRigs()
{
    QSqlDatabase d = db("wspr");
    if (!ensureRigTable(this, d, [this](QSqlDatabase &x, const QString &sql) { return exec(x, sql); }))
        return std::nullopt;
    QSqlQuery q(d);
    if (!q.exec("SELECT * FROM rigs WHERE id > 0 ORDER BY id")) {
        setError(q.lastError().text());
        return std::nullopt;
    }
    QVector<OtherRig> out;
    while (q.next()) {
        const QSqlRecord rec = q.record();
        auto str = [&](const char *c) { return rec.indexOf(c) >= 0 ? q.value(c).toString() : QString(); };
        OtherRig r;
        r.id = q.value("id").toInt();
        r.name = str("rigname");
        r.type = q.value("type").toInt();
        r.selected = q.value("selected").toInt();
        r.txCommand = str("TXcommand");
        r.rxCommand = str("RXcommand");
        r.txPtt = str("TXptt");
        r.protocol = str("Protocol");
        r.port = str("Port");
        r.ip = str("IP");
        r.baud = str("Baud");
        r.serial = str("Serial");
        r.url = str("URL");
        r.checkCommand = str("command1");
        r.checkReply = str("reply1");
        out << r;
    }
    return out;
}

bool SlotStore::saveOtherRig(const OtherRig &r)
{
    QSqlDatabase d = db("wspr");
    if (!ensureRigTable(this, d, [this](QSqlDatabase &x, const QString &sql) { return exec(x, sql); }))
        return false;
    return exec(d, "INSERT INTO rigs(id, rigname, type, selected, TXcommand, RXcommand, TXptt, Protocol, Port, IP, Baud, "
                   "Serial, URL, command1, reply1) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
                   "rigname = VALUES(rigname), type = VALUES(type), selected = VALUES(selected), TXcommand = VALUES(TXcommand), "
                   "RXcommand = VALUES(RXcommand), TXptt = VALUES(TXptt), Protocol = VALUES(Protocol), Port = VALUES(Port), "
                   "IP = VALUES(IP), Baud = VALUES(Baud), Serial = VALUES(Serial), URL = VALUES(URL), "
                   "command1 = VALUES(command1), reply1 = VALUES(reply1)",
                {r.id, r.name, r.type, r.selected, r.txCommand, r.rxCommand, r.txPtt, r.protocol, r.port, r.ip, r.baud,
                 r.serial, r.url, r.checkCommand, r.checkReply});
}

bool SlotStore::deleteOtherRig(int id)
{
    return exec(db("wspr"), "DELETE FROM rigs WHERE id = ?", {id});
}

bool SlotStore::saveSelectedOtherRig(int id)
{
    return exec(db("wspr"), "UPDATE rigs SET selected = ?", {id});
}
