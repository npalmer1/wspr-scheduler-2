#include "repeatplanner.h"

namespace {

constexpr int kMaxDays = 400;   // guard against a mistyped end year

// Round a time up to the next even minute (WSPR slots start on even minutes)
QTime roundUpEven(const QTime &t)
{
    return (t.minute() % 2) ? t.addSecs(60) : t;
}

// Hours from startHour onward at minute mm: the first is always taken, then each
// following hour while (inclusive ? <= : <) limit, stopping at midnight.
void addHourly(SlotTimes &out, const QDate &d, int startHour, int mm, const QTime &limit, bool inclusive)
{
    for (int h = startHour; h < 24; ++h) {
        const QTime t(h, mm);
        if (h != startHour && (inclusive ? t > limit : t >= limit))
            break;
        out.append({d, t});
    }
}

} // namespace

SlotTimes planSlotTimes(const RepeatRequest &req, const SunLookup &sun, QString *error)
{
    SlotTimes out;
    auto fail = [&](const QString &e) { if (error) *error = e; return SlotTimes{}; };

    if (!req.startDate.isValid() || !req.slotTime.isValid())
        return fail("Invalid slot date/time");

    // Single slot: not repeating, "only this slot", or end date before start
    if (!req.repeat || req.thisSlotOnly || req.endDate < req.startDate) {
        out.append({req.startDate, req.slotTime});
        return out;
    }
    if (req.startDate.daysTo(req.endDate) > kMaxDays)
        return fail(QString("Repeat range is more than %1 days").arg(kMaxDays));

    const int mm = req.slotTime.minute();

    if (req.rptType == 2 || req.rptType == 3) {
        const bool night = req.rptType == 3;
        const int off = req.greyOffset;
        for (QDate d = req.startDate; d <= req.endDate; d = d.addDays(1)) {
            const auto w = sun(d);
            if (!w)
                return fail("No sunrise/sunset times - see TX Config");
            if (!night) {
                // Day: from the hour of (sunrise - offset) to (sunset + offset)
                const QTime start = w->rise.addSecs(-off * 60);
                const QTime end = roundUpEven(w->set.addSecs(off * 60));
                addHourly(out, d, start.hour(), mm, end, true);
            } else {
                // Night: midnight to (sunrise + offset), then (sunset - offset) to midnight
                const QTime morningEnd = roundUpEven(w->rise.addSecs(off * 60));
                const int before = out.size();
                addHourly(out, d, 0, mm, morningEnd, true);
                const int lastMorningHour = out.size() > before ? out.last().second.hour() : -1;
                const QTime evening = w->set.addSecs(-off * 60);
                // (polar summer: windows can overlap - don't write an hour twice)
                const int eveningStart = qMax(evening.hour(), lastMorningHour + 1);
                if (eveningStart < 24)
                    addHourly(out, d, eveningStart, mm, QTime(23, 59), true);
            }
        }
        return out;
    }

    int startHour = req.slotTime.hour();
    QTime limit = req.slotTime;           // type 0: once per day at the slot time
    if (req.rptType == 1) {
        if (!req.endTime.isValid() || req.endTime < req.slotTime)
            return fail("Slot end time is before slot time");
        limit = req.endTime;
    } else if (req.rptType == 4) {
        startHour = 0;
        limit = QTime(23, 59);
    }
    for (QDate d = req.startDate; d <= req.endDate; d = d.addDays(1))
        addHourly(out, d, startHour, mm, limit, false);
    return out;
}
