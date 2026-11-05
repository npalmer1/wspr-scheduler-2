#pragma once
// The decode grid of the receive window (C#: RXForm.dataGridView1, 13 columns).
// Rows keep the raw values, so changing the CW/SSB comparison power now updates the last two
// columns of every row (C# only changed the headings; old rows kept the old verdicts).
#include "rxspot.h"

#include <QAbstractTableModel>
#include <QVector>

class RxTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { DateTime, TxCall, Frequency, Snr, Drift, DBm, Watts, Locator, Km, Miles, Az, Cw, Ssb, ColumnCount };
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit RxTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    void setRows(const QVector<RxSpot> &rows);
    void prepend(const RxSpot &s);                  // C#: AddDecodeToGridTop
    void removeAt(int row);
    const RxSpot &spot(int row) const { return m_rows.at(row); }
    void setMaxRows(int n) { m_max = n; }
    void setCompareWatts(int watts);                // CW/SSB comparison power (C#: cwssblistBox)
    int compareWatts() const { return m_watts; }

    QString cellText(int row, int col) const;

private:
    QVector<RxSpot> m_rows;
    int m_max = 2000;
    int m_watts = 100;
    int m_compareDbm = 50;
};
