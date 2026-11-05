#include "rxtablemodel.h"

namespace {
const char *kHeadings[] = {"Date/time", "TX call", "Frequency", "SNR", "drift", "dBm", "W", "Locator",
                           "km", "mls", "Az", "CW @100W", "SSB @100W"};
}

RxTableModel::RxTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int RxTableModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }
int RxTableModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : ColumnCount; }

QString RxTableModel::cellText(int row, int col) const
{
    const RxSpot &d = m_rows.at(row);
    switch (col) {
    case DateTime: return d.datetime.toString("yyyy-MM-dd HH:mm");
    case TxCall: return d.txSign;
    case Frequency: return QString::number(d.frequency, 'f', 6);
    default: break;
    }
    if (d.isNil())
        return QString();   // C#: columns 3..12 blank for "nil rcvd"
    switch (col) {
    case Snr: return Rx::snrText(d.snr);
    case Drift: return QString::number(d.drift);
    case DBm: return QString::number(d.power);
    case Watts: return Rx::wattsText(d.power);
    case Locator: return d.txLoc;
    case Km: return d.distance > -1 ? QString::number(d.distance) : QString();
    case Miles: return d.distance > -1 ? QString::number(Rx::kmToMiles(d.distance)) : QString();
    case Az: return d.azimuth > -1 ? QString::number(d.azimuth) : QString();
    case Cw: return Rx::cwLabel(d.snr, d.power, m_compareDbm);
    case Ssb: return Rx::ssbLabel(d.snr, d.power, m_compareDbm);
    default: return QString();
    }
}

QVariant RxTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const int col = index.column();
    if (role == Qt::DisplayRole)
        return cellText(index.row(), col);
    if (role == Qt::TextAlignmentRole) {
        if (col == Snr)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (col == Drift || col == DBm || col == Watts)
            return int(Qt::AlignCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == SortRole) {   // numbers sort as numbers (C# sorted the text, so "900" > "10000" km)
        const RxSpot &d = m_rows.at(index.row());
        switch (col) {
        case DateTime: return d.datetime;
        case Frequency: return d.frequency;
        case Snr: return d.isNil() ? -999 : d.snr;
        case Drift: return d.isNil() ? -999 : d.drift;
        case DBm: case Watts: return d.isNil() ? -999 : d.power;
        case Km: case Miles: return d.isNil() ? -1 : d.distance;
        case Az: return d.isNil() ? -1 : d.azimuth;
        case Cw: case Ssb: return d.isNil() ? -999 : d.snr - d.power;
        default: return cellText(index.row(), col);
        }
    }
    return {};
}

QVariant RxTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o == Qt::Vertical)
        return role == Qt::DisplayRole ? QVariant(QString()) : QVariant();
    if (role != Qt::DisplayRole || section < 0 || section >= ColumnCount)
        return {};
    if (section == Cw)
        return QString("CW @%1W").arg(m_watts);
    if (section == Ssb)
        return QString("SSB @%1W").arg(m_watts);
    return QString::fromLatin1(kHeadings[section]);
}

void RxTableModel::setRows(const QVector<RxSpot> &rows)
{
    beginResetModel();
    m_rows = rows.mid(0, m_max);
    endResetModel();
}

void RxTableModel::prepend(const RxSpot &s)
{
    beginInsertRows({}, 0, 0);
    m_rows.prepend(s);
    endInsertRows();
    if (m_rows.size() > m_max) {
        beginRemoveRows({}, m_max, m_rows.size() - 1);
        m_rows.resize(m_max);
        endRemoveRows();
    }
}

void RxTableModel::removeAt(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    beginRemoveRows({}, row, row);
    m_rows.remove(row);
    endRemoveRows();
}

void RxTableModel::setCompareWatts(int watts)
{
    m_watts = watts > 0 ? watts : 100;
    m_compareDbm = Rx::wattsToDbm(m_watts);
    emit headerDataChanged(Qt::Horizontal, Cw, Ssb);
    if (!m_rows.isEmpty())
        emit dataChanged(index(0, Cw), index(m_rows.size() - 1, Ssb));
}
