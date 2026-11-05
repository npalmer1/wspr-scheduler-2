#include "slottablemodel.h"

#include <QBrush>
#include <QColor>
#include <QFont>

namespace {
const QString kTick = QStringLiteral("√");   // C#: "   √"
const QString kCross = QStringLiteral("x");

// C#: findendTType()
QString endTimeText(const SlotRecord &r)
{
    if (!r.repeating)
        return "once";
    switch (r.rptType) {
    case RptUntilTime: return r.endTime.toString("HH:mm");
    case RptDay: return "Day+/-";
    case RptNight: return "Night+/-";
    case RptAllDay: return "24hr";
    default: return "1hr";
    }
}
} // namespace

SlotTableModel::SlotTableModel(QObject *parent)
    : QAbstractTableModel(parent), m_slots(kRows)
{
}

int SlotTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : kRows;
}

int SlotTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SlotTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (o == Qt::Vertical)
        return QVariant();   // row header kept blank, as in the C# grid
    static const char *names[ColumnCount] = {"Date", "Time", "Frequency", "Offset", "Pwr dBm", "Pwr W",
                                             "Antenna", "Tuner", "Switch", "End", "Rpt", "Rtype",
                                             "endT", "rptT", "Goff", "Active", "Slot", "Mtype"};
    return section >= 0 && section < ColumnCount ? QString(names[section]) : QVariant();
}

QVariant SlotTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= kRows)
        return {};
    const int col = index.column();

    if (role == Qt::TextAlignmentRole) {
        if (col == ColDate || col == ColTime || col == ColFreq || col == ColAntenna || col == ColEnd)
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        return int(Qt::AlignCenter);
    }

    const auto &s = m_slots[index.row()];
    if (role == Qt::ForegroundRole && col == ColActive && s)
        return QBrush(s->active ? QColor(Qt::red) : QColor(Qt::blue));
    if (role == Qt::FontRole && col == ColActive && s) {
        QFont f;
        f.setBold(true);
        return f;
    }
    if (role != Qt::DisplayRole)
        return {};

    if (col == ColDate)
        return m_date.toString("yyyy-MM-dd");
    if (col == ColTime)
        return rowTime(index.row()).toString("HH:mm");
    if (!s)
        return {};
    switch (col) {
    case ColFreq: return QString::number(s->freq, 'g', 10);
    case ColOffset: return s->offset;
    case ColdBm: return s->powerdBm;
    case ColWatts: return QString::number(s->powerW, 'g', 10);
    case ColAntenna: return s->antenna;
    case ColTuner: return s->tuner;
    case ColSwitch: return s->sw;
    case ColEnd: return s->endDate.toString("yyyy-MM-dd");
    case ColRpt: return s->repeating ? kTick : kCross;
    case ColRptType: return s->rptType;
    case ColEndT: return endTimeText(*s);
    case ColRptTime: return s->rptTime ? "1" : "0";
    case ColGreyOff: return s->greyOffset;
    case ColActive: return s->active ? kTick : kCross;
    case ColSlot: return s->slotNo;
    case ColMsgType: return s->msgType;
    default: return {};
    }
}

void SlotTableModel::setHour(const QDate &date, int hour, const QMap<QTime, SlotRecord> &hourSlots)
{
    beginResetModel();
    m_date = date;
    m_hour = hour;
    for (int r = 0; r < kRows; ++r) {
        const auto it = hourSlots.constFind(rowTime(r));
        if (it != hourSlots.constEnd())
            m_slots[r] = *it;
        else
            m_slots[r].reset();
    }
    endResetModel();
}

int SlotTableModel::rowForTime(const QTime &t) const
{
    if (!t.isValid() || t.hour() != m_hour)
        return -1;
    return t.minute() / 2;
}

int SlotTableModel::defaultWidth(int column)
{
    static const int w[ColumnCount] = {70, 50, 70, 40, 56, 56, 120, 46, 46, 70, 38, 40, 48, 42, 40, 42, 40, 40};
    return column >= 0 && column < ColumnCount ? w[column] : 50;
}
