// Type 2/3 encoding and the slot message rules (port of WsprTransmission.cs) - no database needed.
// Reference symbols were produced by WSJT-X's own wsprcode; the hash value by WSJT-X wsprd's hashtable.
#include "wsprmessage.h"

#include <QCoreApplication>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct Ref { const char *message, *symbols; };
static const Ref kWsprcode[] = {
    {"PJ4/K1ABC 37", "310220001022131020100123131220220230030322022010130031010003323222013010301210032032112203323030223022021023001310310031230021332000010120112222222132323102011022"},
    {"G4GCI/P 23", "310000201222133022122301111222220232010302022232132231210203323220033212323010012010330021123032201002023221021312110213230003132020030322330220220112301300031222"},
    {"G4GCI/7 10", "330202221222113020122103111022020030030100002030112233210201323022033210323010012212330221123210221000003221021312110013230003312220030322330220220112301300013022"},
    {"G4GCI/12 20", "330000201022131220122101111222000030010102002010132033210003321020013212303010012010330223103032221202023021003112110033230201112222030322330000220312301102033222"},
    {"F/G4GCI 23", "330000001022113020102101111020220030012100002032112231010221301220013210303212012210332021303032201202023021023312130233230201132022032322310000220310101100013220"},
    {"EA8/G4GCI 27", "330000001020113222102301111020220230032102002010132231230023301022033230323210012210310021303210201200203023003310130231210201312020030320310202200110321100011020"},
    {"3D2/G4GCI 50", "310200021020113020122301133020200230012100002232112233010003323222033030323212212210312021303230201002003221021110130033212001132022032120310222200110301322011022"},
    {"G4GCI/P 25", "310202201222113222102103131020200232010300022030132233230201323220033210303210032010330023103232201000023221021112110213230001132020030322330222220112301302011222"},
    {"<G4GCI/P> IO90GU 23", "330000221000111202100101331222002230210322200232312013212003101002213232101212232210312201101030001000001221221112112011232201112222032122332222220132301122213020"},
    {"<PJ4/K1ABC> FK52UD 37", "332022223002133202300303131220222012032300200010310013210203103000211010103230210010130021123032201202221203021310130211012201112222032122310020000310101100011202"},
    {"<G4GCI> IO90GU 37", "330200001202111200100301313022022030212322200030312213012201103000213010121212212010330001321012021200001021223310132231210201112222010320332000200132301102211220"},
    {"<K1ABC> FN42AX 0", "332222003222333020302303113022222010210122222032132213012223103202011030303032210230112221303230201220203021201332130211012023332000230320112222200310101120211000"},
    {"G4GCI IO90 23", "330002201220113222102101133020020230030300022230132033010221301222013032303010232010332223123010201002023223021312130031232201132220030320310000220112121120033022"},
};

static QString str(const WsprEncode::Symbols &s)
{
    QString r;
    for (uint8_t v : s) r += QChar('0' + v);
    return r;
}

static TxMessage plan(int msgType, int slot, const char *call, const char *loc, bool one = false, bool t2 = true,
                      bool long6 = true, int pwr = 23)
{
    TxMessageRequest r;
    r.callsign = call;
    r.baseCall = "G4GCI";
    r.locator = loc;
    r.powerdBm = pwr;
    r.msgType = msgType;
    r.slotNo = slot;
    r.oneMsg = one;
    r.allowType2 = t2;
    r.longLocator = long6;
    return planMessage(r);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    for (const Ref &r : kWsprcode) {
        QString err;
        const auto s = WsprEncode::encodeMessage(r.message, &err);
        if (!s || str(*s) != r.symbols)
            std::printf("     mismatch for '%s' %s\n", r.message, qPrintable(err));
        CHECK(s && str(*s) == QString::fromLatin1(r.symbols));
    }
    CHECK(WsprEncode::callHash("G4GCI/P") == 29115);   // as stored by wsprd in hashtable.txt
    CHECK(!WsprEncode::encodeMessage("VP2E/K1ABC 40"));  // 4-char prefix: wsprcode truncates to VP2/, we refuse
    CHECK(!WsprEncode::encodeMessage("<G4GCI> IO90 23")); // type 3 needs 6 chars

    // slot rules
    auto expect = [](const TxMessage &m, const char *text, int type) {
        if (!m.ok() || m.text != text || m.wsprType != type)
            std::printf("     got '%s' type %d err '%s', want '%s' type %d\n", qPrintable(m.text), m.wsprType,
                        qPrintable(m.error), text, type);
        return m.ok() && m.text == text && m.wsprType == type
               && str(*m.symbols) == str(*WsprEncode::encodeMessage(text));
    };
    CHECK(expect(plan(1, 1, "G4GCI/P", "IO90GU"), "G4GCI IO90 23", 1));          // type 1 uses the base call
    CHECK(expect(plan(2, 1, "G4GCI/P", "IO90GU"), "G4GCI/P 23", 2));
    CHECK(expect(plan(2, 2, "G4GCI/P", "IO90GU"), "<G4GCI/P> IO90GU 23", 3));
    CHECK(expect(plan(3, 1, "G4GCI", "IO90GU"), "G4GCI IO90 23", 1));            // 6-char locator, standard call
    CHECK(expect(plan(3, 2, "G4GCI", "IO90GU"), "<G4GCI> IO90GU 23", 3));
    CHECK(expect(plan(3, 1, "G4GCI/P", "IO90GU"), "G4GCI/P 23", 2));             // compound + 6-char loc -> 3
    CHECK(expect(plan(3, 1, "G4GCI/P", "IO90GU", true), "G4GCI/P 23", 2));       // as one tx'n
    CHECK(expect(plan(3, 1, "G4GCI", "IO90GU", true), "G4GCI IO90 23", 1));
    CHECK(expect(plan(2, 1, "G4GCI/P", "IO90GU", false, true, true, 25), "G4GCI/P 27", 2));   // power rounded
    CHECK(!plan(3, 2, "G4GCI", "IO90GU", true).ok());                           // no slot 2 when "as one"
    CHECK(!plan(2, 2, "G4GCI/P", "IO90").ok());                                  // type 3 needs 6 chars
    CHECK(!plan(2, 2, "G4GCI/P", "IO90GU", false, true, false).ok());            // ...and "Allow 6 char locator"
    CHECK(!plan(2, 2, "G4GCI/P", "IO90GU", false, false).ok());                  // type 2/3 disabled
    CHECK(expect(plan(2, 1, "G4GCI/P", "IO90GU", false, false), "G4GCI IO90 23", 1));   // disabled -> type 1
    CHECK(!plan(2, 1, "G4GCIXYZ", "IO90").ok());                                 // long call without / or prefix
    CHECK(!plan(1, 1, "", "IO90").ok());

    // plain-English descriptions
    {
        TxMessageRequest r;
        r.callsign = "G4GCI"; r.baseCall = "G4GCI"; r.locator = "IO90GU"; r.powerdBm = 37;
        r.msgType = 1; r.slotNo = 1;
        auto d = describeMessage(planMessage(r), r);
        CHECK(d == "Type 1 (standard): callsign G4GCI, locator IO90 (4-character square), power 37 dBm = 5 W.");
        r.allowType2 = true; r.longLocator = true; r.oneMsg = false; r.msgType = 3;
        d = describeMessage(planMessage(r), r);
        CHECK(d.startsWith("Type 1 (standard)") && d.contains("IO90GU follows in the next slot (type 3)"));
        r.slotNo = 2;
        d = describeMessage(planMessage(r), r);
        CHECK(d.startsWith("Type 3: callsign sent only as a hash <G4GCI>") && d.contains("locator IO90GU, power 37 dBm = 5 W"));
        r.callsign = "G4GCI/P"; r.slotNo = 1; r.msgType = 2;
        d = describeMessage(planMessage(r), r);
        CHECK(d.startsWith("Type 2: compound callsign G4GCI/P, power 37 dBm = 5 W") && d.contains("next slot"));
        r.oneMsg = true; r.slotNo = 2;
        CHECK(describeMessage(planMessage(r), r).startsWith("Nothing is sent: Second slot not used"));
        std::printf("     %s\n", qPrintable(d));
    }
    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures ? 1 : 0;
}
