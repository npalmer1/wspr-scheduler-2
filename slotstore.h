#pragma once
// All MySQL access needed by the Slots tab.
// Replaces the ~25 C# methods that each opened their own MySqlConnection.
#include "dbbackoff.h"
#include "rigctld.h"
#include "slotrecord.h"

#include <QHash>
#include <QMap>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct DbSettings
{
    QString host = "127.0.0.1";
    int port = 3306;
    QString user = "admin";
    QString password = "wspr";
};

struct AntennaInfo
{
    int antNo = 0;
    QString name;
    QString description;
    int sw = 0, tuner = 0, swPort = 0, sw2 = 0, swPort2 = 0;
};

// wspr.switches / wspr.tuners / wspr.filters (C#: Form1.Hardware)
struct HardwareInfo
{
    int id = 0;                // list position; 0 = "No switch/tuner/filter"
    QString name;
    QString protocol;          // "Serial", "TCP", "UDP" or "" (not set up)
    QString port;              // COM port, or TCP/UDP port number
    QString ip;
    QString baud;
    QString serial;            // "data,parity,stop,flow", e.g. "8,none,1,none"
    QString type;              // text of the type list entry, e.g. "W-410A (antenna selectable by commands)"
    int channels = 1;          // antenna ports on a switch
};

// wspr.rigs - a "Non Rigctl TX" rig (C#: struct Rigs). Commands may contain freq, call, grid, pwr, offset.
struct OtherRig
{
    int id = 0;                // list position; 0 = "None specified"
    QString name;
    int type = 1;
    int selected = 0;          // the rig in use (the same value is kept in every row, as C#)
    QString txCommand;         // sets the rig up for the slot (C#: TXcommand)
    QString rxCommand;         // back to receive / stop (C#: RXcommand)
    QString txPtt;             // start transmitting (C#: TXptt), blank if none
    QString protocol = "TCP";  // "Serial", "TCP" or "UDP" (anything but Serial is sent as HTTP, as C#)
    QString port;              // COM port, or TCP port
    QString ip;
    QString baud;
    QString serial;            // "data,parity,stop,flow"
    QString url;               // API path after http://ip:port/ (C#: URL)
    QString checkCommand;      // new: "TX" when a reply to the TX command is expected (column command1)
    QString checkReply;        // ... it must contain this, any case, e.g. "band set to" (column reply1)
};

struct FrequencyLevel     // wspr.frequencies: TX audio level for one WSPR frequency
{
    double mhz = 0.0;
    double level = 1.0;    // 0..1 (C# trackBar1 / 100)
};

// The subset of wspr_configs.settings the Slots tab needs (C#: ReadConfig)
struct AppConfig
{
    int configId = 0;
    QString callsign = "AZ1AAA";
    QString baseCall = "AZ1AAA";
    QString locator = "IJ90";
    bool longLocator = false;
    double defaultF = 14.0956;
    int defaultOffset = 50;
    int defaultdBm = 0;
    QString defaultAnt;
    bool allowType2 = false;
    bool oneMsg = false;      // "Send type 2/3 as one tx'n" (default off: type 2/3 as two transmissions)
    bool localTime = false;   // TimeZone == "LT"
    bool stopRX = false;      // receive window off
    int selectedFilter = 0;   // settings.selectedFilter
    double alpha = 0.1;       // settings.Alpha - Gaussian filter alpha for the TX audio (C#: defaultAlpha)
    QString hamlibPath;       // folder with rigctl/rigctld (settings.HamlibPath; C# default C:\\WSPR_Sked)
    QString slotDb = "wspr_slots";
};

struct SunTimes
{
    QTime rise;   // local time, as stored by the C# app
    QTime set;
};

class SlotStore
{
public:
    explicit SlotStore(const DbSettings &s = {});
    ~SlotStore();

    void setSettings(const DbSettings &s);
    const DbSettings &settings() const { return m_settings; }

    // Slot database ("wspr_slots" or "wspr_slots_test")
    QString slotDbName() const { return m_slotDb; }
    void setSlotDbName(const QString &name);
    bool slotDbReachable();
    bool databaseExists(const QString &name);
    bool createSlotDatabase(const QString &name);
    void ensureIndexes();
    bool hasSwitch2Columns();   // C#: HasNewSlotColumns (cached)

    // Reading
    // One query per grid page (C# ran 30 queries, one connection each). nullopt = database error.
    std::optional<QMap<QTime, SlotRecord>> loadHour(const QDate &date, int hour);
    std::optional<SlotRecord> find(const QDate &date, const QTime &time);

    // Writing. upsertMany runs in one transaction (C# opened a connection per row).
    bool upsert(const SlotRecord &r);
    bool upsertMany(const QVector<SlotRecord> &rows);
    bool deleteOne(const QDate &date, const QTime &time);
    bool deleteByParent(const QString &parent);
    bool deleteBetween(const QDate &from, const QDate &to);   // inclusive
    bool deleteBefore(const QDate &cutoff);

    // Other databases used by the Slots tab
    QVector<AntennaInfo> loadAntennas();                       // wspr.antennas
    bool saveAntenna(const AntennaInfo &a);                    // C#: SaveAntenna / UpdateAntenna
    bool deleteAntenna(int antNo);                             // C#: DeleteAntenna
    // C#: UpdateSlotAntennas - slots from fromDate on that use oldName get newName
    bool renameAntennaInSlots(const QString &oldName, const QString &newName, const QDate &fromDate);
    // table: "switches", "tuners" or "filters" (C#: ReadHardware / SaveHardware / UpdateHardware)
    std::optional<QVector<HardwareInfo>> loadHardware(const QString &table);
    bool saveHardware(const QString &table, const HardwareInfo &h);
    bool saveSelectedFilter(int configId, int filter);         // wspr_configs.settings.selectedFilter
    // C#: ReadRigs / SaveRigInfo / DeleteOtherTX / createRigTable
    std::optional<QVector<OtherRig>> loadOtherRigs();          // rows with id > 0, by id
    bool saveOtherRig(const OtherRig &r);
    bool deleteOtherRig(int id);
    bool saveSelectedOtherRig(int id);                         // rigs.selected in every row
    QVector<FrequencyLevel> loadFrequencies();                 // C#: ReadFrequencies
    bool saveFrequency(const FrequencyLevel &f);               // C#: SaveFrequency / UpdateFrequency
    bool deleteFrequency(double mhz);                          // C#: DeleteFrequency
    std::optional<AppConfig> loadConfig();                     // wspr_configs.settings
    bool saveSlotDbName(int configId, const QString &name);    // wspr_configs.settings.SlotDB
    bool saveHamlibPath(int configId, const QString &path);    // wspr_configs.settings.HamlibPath
    std::optional<RigctlSettings> loadRigctl();                // wspr.rigctl (C#: readRigctl)
    bool saveRigctl(const RigctlSettings &r);                  // C#: SaveRigctl / UpdateRigctl
    std::optional<SunTimes> sunTimes(const QString &locator4, const QDate &date);   // wspr_grey.sunrise_sunset
    QHash<QString, SunTimes> allSunTimes(const QString &locator4);                 // keyed "MM-dd"
    // C#: Save_SunSet_SunRise for a whole year, in one transaction (creates wspr_grey if missing)
    bool saveSunTimes(const QString &locator4, const QMap<QString, SunTimes> &days);
    bool saveAlpha(int configId, double alpha);
    // Changes the MySQL/MariaDB password of the account this connection logs in as (C#: setAllPasswords,
    // which ran ALTER USER 'admin'@'%' six times and ignored errors). Tries ALTER USER CURRENT_USER(),
    // then SET PASSWORD = PASSWORD(..) (MariaDB: changing your own password needs no privilege this way),
    // then SET PASSWORD = '..' (MySQL 8). The password must be letters, digits and . , - _ space.
    bool changeOwnPassword(const QString &newPassword);
    // A fresh connection with these settings works
    static bool canLogIn(const DbSettings &s, QString *error = nullptr);                // wspr_configs.settings.Alpha
    // C#: SaveConfig - call, locator, defaults (wspr_configs.settings; a row is added if there is none)
    bool saveSettings(const AppConfig &c, int msgType);

    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase db(const QString &database);   // opens or reuses a named connection, reconnecting if dropped
    bool columnExists(QSqlDatabase &d, const QString &table, const QString &column);
    bool addColumnIfMissing(QSqlDatabase &d, const QString &table, const QString &column, const QString &type);
    bool exec(QSqlDatabase d, const QString &sql, const QVariantList &binds = {});
    void setError(const QString &e) { m_lastError = e; }

    DbSettings m_settings;
    QString m_slotDb = "wspr_slots";
    QString m_connPrefix;
    DbBackoff m_backoff;       // no new connection attempts for a while after one failed
    QStringList m_connections;
    std::optional<bool> m_hasSwitch2;
    QString m_lastError;
};
