#pragma once
// One row of the `slots` table (C#: struct SlotData).
#include <QDate>
#include <QString>
#include <QTime>

struct SlotRecord
{
    QDate date;
    QTime time;            // even minute, HH:mm
    double freq = 0.0;     // MHz, rounded to 4 dp
    int offset = 0;        // Hz
    int powerdBm = 0;
    double powerW = 0.0;
    QString antenna;
    int tuner = 0;
    int sw = 0;            // C#: Switch
    int swPort = 0;        // C#: SwitchPort (stored as shown-in-editor + 1, as the C# app does)
    int sw2 = 0;           // C#: Switch2
    int swPort2 = 0;       // C#: SwitchPort2
    QDate endDate;         // C#: End
    bool active = false;
    bool repeating = false;
    QTime endTime;         // C#: TimeEnd
    bool rptTime = false;  // C#: RptTime  (repeat until end time)
    QString parent;        // "yyyy-MM-dd HH:mm" of the slot that created this repeat set
    int slotNo = 1;        // 1, or 2 for the second half of a type 2/3 pair
    int msgType = 1;
    int rptType = 0;       // 0 none/daily, 1 until end time, 2 day, 3 night, 4 24hr
    int greyOffset = 0;    // minutes added around sunrise/sunset for rptType 2/3
};

// Repeat types (C#: rptType)
enum RepeatType { RptNone = 0, RptUntilTime = 1, RptDay = 2, RptNight = 3, RptAllDay = 4 };

// C# dBtoWatts(): only the standard WSPR power steps are valid
inline QString dBmToWatts(int dBm)
{
    switch (dBm) {
    case 0: return "0.001";  case 3: return "0.002";  case 7: return "0.005";
    case 10: return "0.01";  case 13: return "0.02";  case 17: return "0.05";
    case 20: return "0.1";   case 23: return "0.2";   case 27: return "0.5";
    case 30: return "1";     case 33: return "2";     case 37: return "5";
    case 40: return "10";    case 43: return "20";    case 47: return "50";
    case 50: return "100";   case 53: return "200";   case 57: return "500";
    case 60: return "1000";
    default: return QString();
    }
}
