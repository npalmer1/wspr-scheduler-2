#include "wsprmessage.h"
#include "slotrecord.h"

namespace {

int roundedPower(int p)   // what types 2/3 actually carry; type 1 rounds down (SanitizePower)
{
    static const int nu[10] = {0, -1, 1, 0, -1, 2, 1, 0, -1, 1};
    p = qBound(0, p, 60);
    return p + nu[p % 10];
}

TxMessage type1(const QString &call, const QString &loc, int power)
{
    TxMessage m;
    m.wsprType = 1;
    const QString loc4 = loc.left(4).toUpper();
    m.text = QString("%1 %2 %3").arg(call.trimmed().toUpper(), loc4).arg(WsprEncode::sanitizePower(power));
    m.symbols = WsprEncode::encodeType1(call, loc4, power, &m.error);
    return m;
}

TxMessage type2(const QString &call, int power)
{
    TxMessage m;
    m.wsprType = 2;
    m.text = QString("%1 %2").arg(call.trimmed().toUpper()).arg(roundedPower(power));
    m.symbols = WsprEncode::encodeType2(call, power, &m.error);
    return m;
}

TxMessage type3(const QString &call, const QString &loc, int power)
{
    TxMessage m;
    m.wsprType = 3;
    m.text = QString("<%1> %2 %3").arg(call.trimmed().toUpper(), loc.toUpper()).arg(roundedPower(power));
    m.symbols = WsprEncode::encodeType3(call, loc, power, &m.error);
    return m;
}

TxMessage refuse(const QString &why)
{
    TxMessage m;
    m.error = why;
    return m;
}

} // namespace

TxMessage planMessage(const TxMessageRequest &r)
{
    const QString call = r.callsign.trimmed().toUpper();
    const QString base = r.baseCall.trimmed().toUpper();
    if (call.isEmpty() || r.locator.trimmed().size() < 4)
        return refuse("Invalid callsign or locator");   // C#: OKMessageBox("Invalid callsign or locator")
    // 6-char locator only when allowed and type 2/3 enabled (C# WSPR_Encode)
    const QString loc = r.locator.trimmed().toUpper();
    const bool loc6 = loc.size() >= 6 && r.longLocator && r.allowType2;
    const QString locFull = loc6 ? loc.left(6) : loc.left(4);

    if (r.testTone || r.msgType <= 1)
        return type1(base.isEmpty() ? call : base, locFull, r.powerdBm);

    if (!r.allowType2)
        return r.slotNo == 2 ? refuse("Type 2 messages not enabled")
                             : type1(base.isEmpty() ? call : base, locFull, r.powerdBm);

    const bool compound = call.contains('/');
    if (r.oneMsg) {
        if (r.slotNo == 2)
            return refuse("Second slot not used when sending type 2/3 as one transmission");
        return compound ? type2(call, r.powerdBm) : type1(call, locFull, r.powerdBm);
    }
    if (r.slotNo == 2) {
        if (!loc6)
            return refuse("Type 3 needs a 6-character locator (and \"Allow 6 char locator\")");
        return type3(call, locFull, r.powerdBm);
    }
    // slot 1. checkCall gives msgType 3 for any 6-char locator, so a compound call can arrive as 3 too.
    if (compound)
        return type2(call, r.powerdBm);
    if (r.msgType == 2)
        return refuse("Type 2 needs a callsign with a prefix or suffix, e.g. G4GCI/P");
    return type1(call, locFull, r.powerdBm);   // standard call; the 6-char locator follows in slot 2 (type 3)
}

QString describeMessage(const TxMessage &m, const TxMessageRequest &req)
{
    if (!m.ok())
        return "Nothing is sent: " + m.error;
    const QStringList t = m.text.split(' ', Qt::SkipEmptyParts);
    const auto power = [](const QString &dbm) {
        const QString w = dBmToWatts(dbm.toInt());
        return w.isEmpty() ? dbm + " dBm" : QString("%1 dBm = %2 W").arg(dbm, w);
    };
    const QString loc = req.locator.trimmed().toUpper();
    const bool twoTx = req.allowType2 && !req.oneMsg;
    switch (m.wsprType) {
    case 1: {
        QString s = QString("Type 1 (standard): callsign %1, locator %2 (%3-character square), power %4.")
                        .arg(t.value(0), t.value(1)).arg(t.value(1).size()).arg(power(t.value(2)));
        if (twoTx && req.slotNo == 1 && loc.size() >= 6 && req.longLocator)
            s += QString(" The 6-character locator %1 follows in the next slot (type 3).").arg(loc.left(6));
        return s;
    }
    case 2: {
        QString s = QString("Type 2: compound callsign %1, power %2. Type 2 has no room for a locator")
                        .arg(t.value(0), power(t.value(1)));
        s += twoTx ? QString(" - the next slot sends it (type 3, with <%1>).").arg(t.value(0))
                   : QString(" - receivers show none, or the one from your earlier messages.");
        return s;
    }
    case 3: {
        QString call = t.value(0);
        call.remove('<').remove('>');
        return QString("Type 3: callsign sent only as a hash <%1> - receivers show %1 if they have decoded "
                       "your type 1 or 2 message recently, otherwise <...>; 6-character locator %2, power %3.")
            .arg(call, t.value(1), power(t.value(2)));
    }
    }
    return m.text;
}
