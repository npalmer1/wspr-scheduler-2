#pragma once
// Which WSPR message to send in a slot - port of WsprTransmission.WsprTxn plus the message-choosing
// part of Form1.WSPR_Encode. Types 2/3 are encoded natively (wsprencode), so wspr_enc.exe and the
// WsprMsg class are no longer needed.
//
// What gets sent (slot msgType comes from checkCall when the slot was saved):
//   msgType 1                    -> type 1: base call, 4-char locator, power
//   msgType 2/3, slot 1, compound -> type 2: compound call (G4GCI/P, PJ4/K1ABC), power
//   msgType 3, slot 1, standard   -> type 1: call, 4-char locator, power     (6-char locator users)
//   msgType 2 or 3, slot 2       -> type 3: <call>, 6-char locator, power
//   "Send type 2/3 as one tx'n"  -> slot 1 only: type 2 for a compound call, otherwise type 1
//     (one WSPR message cannot carry a compound call and a 6-char locator together - see notes)
#include "wsprencode.h"

#include <QString>
#include <optional>

struct TxMessageRequest
{
    QString callsign;          // full call (CalltextBox), may be compound
    QString baseCall;          // base call (baseCalltextBox), used for type 1
    QString locator;           // 4 or 6 characters
    int powerdBm = 0;          // slot power
    int msgType = 1;           // from the slot (1, 2 or 3)
    int slotNo = 1;            // 1, or 2 for the second half of a pair
    bool oneMsg = false;       // asOnecheckBox: "Send type 2/3 as one tx'n"
    bool allowType2 = false;   // Type2checkBox
    bool longLocator = false;  // longcheckBox: "Allow 6 char locator"
    bool testTone = false;     // C# flatcode: test TX button -> slot 1, type 1
};

struct TxMessage
{
    std::optional<WsprEncode::Symbols> symbols;
    QString text;              // WSJT-X style, e.g. "G4GCI/P 23" or "<G4GCI/P> IO90GU 23"
    int wsprType = 0;          // 1, 2 or 3
    QString error;             // set when nothing should be transmitted
    bool ok() const { return symbols.has_value(); }
};

TxMessage planMessage(const TxMessageRequest &req);

// What a planned message tells the receiving stations, in plain English, e.g.
// "Type 1 (standard): callsign G4GCI, locator IO90 (4-character square), power 37 dBm = 5 W."
QString describeMessage(const TxMessage &m, const TxMessageRequest &req);
