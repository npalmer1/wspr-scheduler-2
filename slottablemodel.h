#pragma once
// The slot grid: 30 two-minute slots for one date and hour.
// Replaces the C# DataTable `dtable` + DataGridView cell writes (GridHeading, populateGridSlot, DeleteRow).
#include "slotrecord.h"

#include <QAbstractTableModel>
#include <QMap>
#include <QVector>
#include <optional>

class SlotTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // Same column order as the C# grid (GridHeading)
    enum Column {
        ColDate, ColTime, ColFreq, ColOffset, ColdBm, ColWatts, ColAntenna, ColTuner, ColSwitch,
        ColEnd, ColRpt, ColRptType, ColEndT, ColRptTime, ColGreyOff, ColActive, ColSlot, ColMsgType,
        ColumnCount
    };
    static constexpr int kRows = 30;

    explicit SlotTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;

    // Show an hour: rows are always present, slots fill the ones that exist in the database
    void setHour(const QDate &date, int hour, const QMap<QTime, SlotRecord> &hourSlots);
    QDate date() const { return m_date; }
    int hour() const { return m_hour; }

    QTime rowTime(int row) const { return QTime(m_hour, row * 2); }
    int rowForTime(const QTime &t) const;   // -1 if not in this hour
    const std::optional<SlotRecord> &slot(int row) const { return m_slots[row]; }

    static int defaultWidth(int column);    // C#: setColumnWidth

private:
    QDate m_date;
    int m_hour = 0;
    QVector<std::optional<SlotRecord>> m_slots;
};
