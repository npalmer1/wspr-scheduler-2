#pragma once
// Works out which date/times a slot is written to when it repeats.
// Pure logic (no UI, no database) so it can be tested on its own.
// C#: locateSlotMembersDT (repeat types 0, 1, 4) and locateSlotMembersDT_Sun (types 2, 3).
#include <QDate>
#include <QPair>
#include <QString>
#include <QTime>
#include <QVector>
#include <functional>
#include <optional>

struct RepeatRequest
{
    QDate startDate;          // the slot being saved
    QTime slotTime;           // even minute
    bool repeat = false;      // "Allow repeating slots each day?"
    int rptType = 0;          // 0 same time daily, 1 hourly until endTime, 2 day, 3 night, 4 hourly 24h
    QDate endDate;            // "repeat until"
    QTime endTime;            // only for rptType 1
    int greyOffset = 0;       // minutes, widens the day/night window (types 2 and 3)
    bool thisSlotOnly = false;
};

struct SunWindow { QTime rise, set; };   // in the same time base as the grid (UTC or LT)
using SunLookup = std::function<std::optional<SunWindow>(const QDate &)>;

using SlotTimes = QVector<QPair<QDate, QTime>>;

// Returns the date/times to save, or an empty list with *error set.
SlotTimes planSlotTimes(const RepeatRequest &req, const SunLookup &sun, QString *error = nullptr);
