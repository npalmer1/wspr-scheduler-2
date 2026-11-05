#pragma once
// MySQL access for the receive window (C#: RXForm table_count, Save_Received_DB, find_reported_async,
// find_selected, DeleteRow, Read_Config, Save_Config) plus Form1.Read_Audio (wspr_configs.rxsettings).
//
// Differences from C#:
//  - every value is a bound parameter. C# built the filter and delete SQL by pasting text-box contents
//    into the query, so a pasted call like O'BRIEN broke it (and anything else could be injected);
//  - the band filter uses frequency ranges instead of `frequency LIKE '14%'`, which also matched 2m
//    (144 MHz) and never matched LF (spots are at 0.1375 MHz, the filter looked for '0.136%');
//  - the call filter also finds type 3 decodes, stored as "<G4GCI/P>";
//  - delete matches the frequency to within 0.5 Hz rather than exact floating-point equality;
//  - one connection is kept open and reused (C# opened one per query).
#include "rxspot.h"
#include "slotstore.h"   // DbSettings

#include <QSqlDatabase>
#include <QVariantList>

struct RxConfig          // wspr_rpt.rxconfig (one row, id 0)
{
    bool deep = false;
    bool quick = false;
    int osd = 0;
    bool upload = false;
};

struct RxSettings        // wspr_configs.rxsettings (C#: Form1.Read_Audio)
{
    QString outputName;
    int outputDevice = -1;
    QString inputName;   // C# list text, "1: Microphone (USB Audio Device)"
    int inputDevice = -1;
    int outLevel = 100;
    int inLevel = 100;   // gain = inLevel / 100
    bool sameDevice = false;
    QString wsprdPath;
};

struct RxFilter
{
    bool useDates = false;
    QDateTime from, to;          // UTC, inclusive
    int bandIndex = 0;           // index into Rx::bandRanges(); 0 = all
    QString call;                // prefix; '*' ignored as C#
    std::optional<int> minKm, maxKm;
};

struct RxQuery { QString sql; QVariantList binds; };

class RxStore
{
public:
    explicit RxStore(const DbSettings &s = {}, const QString &database = "wspr_rpt");
    ~RxStore();
    void setSettings(const DbSettings &s);
    void setDatabase(const QString &name) { m_database = name; }   // "wspr_rpt"; tests use a scratch copy

    bool insert(const RxSpot &s);                          // INSERT IGNORE
    std::optional<int> count();
    std::optional<QVector<RxSpot>> latest(int maxRows);    // newest first
    std::optional<QVector<RxSpot>> filtered(const RxFilter &f, int maxRows);
    bool remove(const QDateTime &datetime, const QString &txSign, double frequency);

    std::optional<RxConfig> readConfig();
    bool saveConfig(const RxConfig &c);
    std::optional<RxSettings> readRxSettings();             // from wspr_configs
    bool saveRxSettings(const RxSettings &s);                // C#: Form1.Save_Audio

    QString lastError() const { return m_lastError; }

    // SQL for a filter (public for the tests)
    static RxQuery filterQuery(const RxFilter &f, int maxRows);

private:
    QSqlDatabase db(const QString &database);
    std::optional<QVector<RxSpot>> select(const RxQuery &q);

    DbSettings m_settings;
    QString m_database;
    QString m_connPrefix;
    QStringList m_connections;
    QString m_lastError;
};
